#!/usr/bin/env python3
"""Hands-free controller for Tetris++.

Reads the webcam, tracks one hand with MediaPipe and sends commands to the
game over UDP (127.0.0.1:5005). The game starts it for you (Tetris.exe
--gestures, through gesture\\run_windows.bat). To run it yourself:

    py gesture/hand_control.py                # flick mode (default)
    py gesture/hand_control.py --mode point   # point and hold

Point your index finger up, like showing "1" (other fingers curled in).

Modes
    flick     swipe the finger left / right   move one column
    point     tilt the finger left / right    move, and keep moving while held
    palm      swipe an open hand              the original palm swipes
    position  move your hand sideways         the piece follows it

Finger modes (flick, point)
    dip the finger down and back up  hard drop (or swipe the hand down)
    pinch (thumb to index tip)       rotate
    open palm, held for a second     pause
    curl the finger / relax          nothing: rest any time
    In menus: swipe to move, drop to select, pinch to go up.
Palm and position modes: pinch rotates, a held fist pauses / selects.

Keys in the preview window: m = next mode, q / Esc = quit

Tuning
    --stats            print frame rate, tracking time and lost-hand counts
    --record clip.jsonl  save every frame's hand landmarks and the commands
                       sent; replay.py runs the detector over such a file
    --backend dshow    open the camera through DirectShow instead of the default
"""
from __future__ import annotations

import argparse
import dataclasses
import json
import os
import socket
import sys
import threading
import time
import urllib.request

# Keep OpenCV quiet: a missing camera otherwise prints pages of warnings
os.environ.setdefault("OPENCV_LOG_LEVEL", "OFF")
os.environ.setdefault("OPENCV_VIDEOIO_DEBUG", "0")

try:
    import cv2
    import mediapipe as mp
    from mediapipe.tasks.python import BaseOptions, vision
except ImportError:
    sys.exit("Missing packages. Install them with:\n"
             "    pip install -r gesture/requirements.txt")

from gestures import FINGER_MODES, MODE_ALIASES, MODES, Config, GestureDetector

MODEL_URL = ("https://storage.googleapis.com/mediapipe-models/hand_landmarker/"
             "hand_landmarker/float16/latest/hand_landmarker.task")
MODEL_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "models", "hand_landmarker.task")

HAND_CONNECTIONS = [(0, 1), (1, 2), (2, 3), (3, 4), (0, 5), (5, 6), (6, 7),
                    (7, 8), (5, 9), (9, 10), (10, 11), (11, 12), (9, 13),
                    (13, 14), (14, 15), (15, 16), (13, 17), (17, 18), (18, 19),
                    (19, 20), (0, 17)]
LABELS = {"L": "LEFT", "R": "RIGHT", "H": "DROP", "U": "ROTATE", "C": "SELECT",
          "P": "PAUSE"}
BACKENDS = ("auto", "dshow", "msmf")
PREVIEW_INTERVAL = 0.06     # seconds between preview redraws: every 2nd frame at 30 fps
STATS_PERIOD = 5.0          # --stats prints one line this often (seconds)
RECORDING_VERSION = 1


def ensure_model():
    if os.path.exists(MODEL_PATH):
        return MODEL_PATH
    os.makedirs(os.path.dirname(MODEL_PATH), exist_ok=True)
    print("Downloading hand model (~8 MB)...")
    urllib.request.urlretrieve(MODEL_URL, MODEL_PATH)
    return MODEL_PATH


CAMERA_HELP = """
No webcam found. Check that a camera is connected and not used by another
app, or pick one with --camera 1 (or a video file with --camera clip.mp4).
"""


def open_camera(source, width, height, backend="auto"):
    """Opens a camera index or a video file. backend: "auto" lets OpenCV
    choose (Media Foundation on Windows), "dshow" / "msmf" force one."""
    # With the default camera, also try the next few indices (USB cams are
    # often 1 or 2)
    candidates = [source] if not isinstance(source, int) or source != 0 else [0, 1, 2, 3]
    for candidate in candidates:
        if backend == "auto" or not isinstance(candidate, int):
            cap = cv2.VideoCapture(candidate)
        else:
            cap = cv2.VideoCapture(candidate, cv2.CAP_DSHOW if backend == "dshow"
                                   else cv2.CAP_MSMF)
        if not cap.isOpened():
            cap.release()
            continue
        if backend == "dshow":
            # DirectShow only reaches 30+ fps with compressed frames
            cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        # Small frames and a 1-frame buffer keep latency low
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        cap.set(cv2.CAP_PROP_FPS, 60)
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        if candidate != source:
            print(f"Using camera {candidate}")
        return cap
    print(f"Could not open camera/video: {source}", file=sys.stderr)
    sys.exit(CAMERA_HELP)


def describe_camera(cap, backend):
    """What the camera really delivers (it may ignore the size and frame rate
    we asked for)."""
    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = cap.get(cv2.CAP_PROP_FPS)
    return f"{w}x{h}, camera reports {fps:.0f} fps ({backend} backend)"


# ------------------------------------------------------------ frame readers

class LatestFrameReader:
    """Reads the camera on its own thread and hands out only the newest
    frame. If tracking falls behind for a moment, the frames it missed are
    skipped instead of queueing up, so the game never reacts to an old
    picture. Frames are stamped when they arrive."""

    def __init__(self, cap, clock):
        self._cap = cap
        self._clock = clock
        self._cond = threading.Condition()
        self._frame = None
        self._stamp = 0.0
        self._arrived = 0      # frames the camera delivered
        self._taken = 0        # the last one read() handed out
        self._ended = False
        self._stop = False
        self.dropped = 0       # frames that were replaced before anyone used them
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        while not self._stop:
            ok, frame = self._cap.read()
            stamp = self._clock.perf_counter()
            with self._cond:
                if not ok:
                    self._ended = True
                    self._cond.notify_all()
                    return
                if self._arrived > self._taken:
                    self.dropped += 1
                self._frame, self._stamp = frame, stamp
                self._arrived += 1
                self._cond.notify_all()

    def read(self):
        """Waits for a frame newer than the last one returned. Returns
        (frame, stamp in seconds), or None when the camera has stopped."""
        with self._cond:
            while self._arrived == self._taken and not self._ended:
                self._cond.wait(0.2)  # wake up regularly so Ctrl+C works
            if self._arrived == self._taken:
                return None
            self._taken = self._arrived
            return self._frame, self._stamp

    def close(self):
        """Stops the thread; call it before releasing the camera."""
        self._stop = True
        self._thread.join(1.0)


class SequentialReader:
    """Hands out every frame in order. Used for video files, which are timed
    by their own frame rate so a clip gives the same result on every run, and
    (with fps=None, timed by the clock) by the tests."""

    dropped = 0

    def __init__(self, cap, clock, fps=None):
        self._cap = cap
        self._clock = clock
        self._fps = fps
        self._index = 0

    def read(self):
        ok, frame = self._cap.read()
        if not ok:
            return None
        stamp = self._index / self._fps if self._fps else self._clock.perf_counter()
        self._index += 1
        return frame, stamp

    def close(self):
        pass


def video_fps(cap):
    """Frame rate of a video file (30 if the file doesn't say)."""
    fps = cap.get(cv2.CAP_PROP_FPS)
    return fps if fps and fps == fps and fps > 1 else 30.0


# ------------------------------------------------------------ stats / record

class Stats:
    """Timing numbers for tuning (--stats): one line every few seconds."""

    def __init__(self, period=STATS_PERIOD):
        self.period = period
        self._start = None
        self._last = None
        self._was_seen = False
        self._dropped_before = 0
        self._reset()

    def _reset(self):
        self._frames = 0
        self._gap_max = 0.0
        self._track_sum = 0.0
        self._track_max = 0.0
        self._seen = 0
        self._lost = 0

    def frame(self, now, tracking_seconds, hand_seen, dropped_total):
        """Call once per processed frame. now: the frame's time in seconds;
        tracking_seconds: how long MediaPipe took; dropped_total: the frame
        reader's count of skipped frames. Returns a line to print when a
        period is complete, otherwise None."""
        if self._start is None:
            # the first frame only starts the clock: rates count the gaps
            # between frames
            self._start = self._last = now
            self._was_seen = hand_seen
            return None
        self._gap_max = max(self._gap_max, now - self._last)
        self._last = now
        self._frames += 1
        self._track_sum += tracking_seconds
        self._track_max = max(self._track_max, tracking_seconds)
        if hand_seen:
            self._seen += 1
        elif self._was_seen:
            self._lost += 1  # the hand was there on the frame before
        self._was_seen = hand_seen

        elapsed = now - self._start
        if elapsed < self.period:
            return None
        line = (f"stats: {self._frames / elapsed:.1f} fps, longest gap "
                f"{self._gap_max * 1000:.0f} ms | tracking "
                f"{self._track_sum / self._frames * 1000:.1f} ms avg, "
                f"{self._track_max * 1000:.0f} ms worst | "
                f"{dropped_total - self._dropped_before} stale frames skipped | hand seen "
                f"{100 * self._seen / self._frames:.0f}% of frames, lost {self._lost} times")
        self._dropped_before = dropped_total
        self._start = now
        self._reset()
        return line


def extract_hand(result):
    """The first hand of a MediaPipe result as plain lists, or None.
    image: 21 (x, y, z) in 0..1 picture coordinates (z = depth relative to
    the wrist); world: 21 (x, y, z) in metres around the hand's centre;
    side: "Left" / "Right" as MediaPipe sees it in the mirrored picture."""
    if not result.hand_landmarks:
        return None
    image = [(p.x, p.y, getattr(p, "z", 0.0) or 0.0) for p in result.hand_landmarks[0]]
    world = None
    worlds = getattr(result, "hand_world_landmarks", None)
    if worlds:
        world = [(p.x, p.y, p.z) for p in worlds[0]]
    side, score = "", 0.0
    handedness = getattr(result, "handedness", None)
    if handedness and handedness[0]:
        side = handedness[0][0].category_name or ""
        score = handedness[0][0].score or 0.0
    return {"image": image, "world": world, "side": side, "score": score}


class Recorder:
    """Writes a recording for replay.py: a header line, then one JSON line
    per frame with the landmarks and the commands the detector sent."""

    def __init__(self, path, detector):
        self._file = open(path, "w", encoding="utf-8")
        header = {"tetris_gesture_recording": RECORDING_VERSION, "mode": detector.mode,
                  "config": dataclasses.asdict(detector.cfg)}
        self._file.write(json.dumps(header) + "\n")

    @staticmethod
    def _rounded(points):
        return [[round(v, 5) for v in p] for p in points] if points else None

    def frame(self, t, hand, commands):
        row = {"t": round(t, 4), "cmd": list(commands)}
        if hand:
            row["lm"] = self._rounded(hand["image"])
            row["world"] = self._rounded(hand["world"])
            row["side"] = hand["side"]
        else:
            row["lm"] = None
        self._file.write(json.dumps(row, separators=(",", ":")) + "\n")

    def close(self):
        self._file.close()


# ------------------------------------------------------------ preview

def draw_preview(frame, landmarks, detector, fps, flash):
    h, w = frame.shape[:2]
    if landmarks:
        pts = [(int(x * w), int(y * h)) for x, y in landmarks]
        for a, b in HAND_CONNECTIONS:
            cv2.line(frame, pts[a], pts[b], (230, 200, 110), 2)
        for p in pts:
            cv2.circle(frame, p, 4, (255, 255, 255), -1)
    if detector.mode == "position":
        m = detector.cfg.position_margin
        for i in range(detector.cfg.columns + 1):
            x = int((m + (1 - 2 * m) * i / detector.cfg.columns) * w)
            cv2.line(frame, (x, 0), (x, 12), (120, 120, 120), 1)
        if detector.last_column is not None:
            x0 = int((m + (1 - 2 * m) * detector.last_column / detector.cfg.columns) * w)
            x1 = int((m + (1 - 2 * m) * (detector.last_column + 1) / detector.cfg.columns) * w)
            cv2.rectangle(frame, (x0, 0), (x1, 12), (110, 200, 255), -1)
    if detector.mode in FINGER_MODES:
        draw_finger_meter(frame, detector)
    cv2.putText(frame, f"{detector.mode} mode  {fps:4.0f} fps  (m: next mode, q: quit)",
                (10, h - 12), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
    if flash and time.perf_counter() - flash[1] < 0.4:
        cv2.putText(frame, flash[0], (10, 50), cv2.FONT_HERSHEY_SIMPLEX, 1.4,
                    (110, 200, 255), 3)


def draw_finger_meter(frame, detector):
    """Shows whether the pointing pose is seen and how far the fingertip has
    swiped (flick) or tilted (point), with the left/right thresholds, so you
    can tune --sensitivity."""
    h, w = frame.shape[:2]
    info = detector.debug
    pointing = info["pointing"]
    cv2.putText(frame, "pointing: yes" if pointing else "pointing: no (point up, curl the others)",
                (10, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.55,
                (110, 230, 110) if pointing else (80, 80, 230), 2)
    cx, y, half = w // 2, h - 40, w // 3
    limit = info["threshold"] * 2  # the meter shows +-2x the threshold
    cv2.line(frame, (cx - half, y), (cx + half, y), (120, 120, 120), 2)
    for sign in (-1, 1):
        tx = cx + int(sign * half / 2)          # the threshold marks
        cv2.line(frame, (tx, y - 10), (tx, y + 10), (110, 200, 255), 2)
        rx = cx + int(sign * half / 4)          # the "back at rest" zone
        cv2.line(frame, (rx, y - 5), (rx, y + 5), (160, 160, 160), 1)
    if pointing:
        pos = max(-1.0, min(1.0, info["dx"] / limit))
        cv2.circle(frame, (cx + int(pos * half), y), 8, (255, 255, 255), -1)


# ------------------------------------------------------------ main loop

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--camera", default="0", help="camera index or video file")
    ap.add_argument("--mode", choices=list(MODES) + list(MODE_ALIASES), default="flick",
                    help="flick (default), point, palm or position")
    ap.add_argument("--port", type=int, default=5005)
    ap.add_argument("--width", type=int, default=640)
    ap.add_argument("--height", type=int, default=360)
    ap.add_argument("--backend", choices=BACKENDS, default="auto",
                    help="how to open the camera: auto (default), dshow or msmf")
    ap.add_argument("--no-preview", action="store_true", help="don't open a window")
    ap.add_argument("--sensitivity", type=float, default=1.0,
                    help=">1 = smaller/slower swipes trigger, <1 = need bigger swipes")
    ap.add_argument("--no-vertical", action="store_true",
                    help="no drop flick / up-down swipes (pinch still rotates, fist pauses)")
    ap.add_argument("--stats", action="store_true",
                    help="print frame rate, tracking time and lost-hand counts every 5 s")
    ap.add_argument("--record", metavar="FILE",
                    help="save landmarks and commands of every frame (for replay.py)")
    ap.add_argument("--sync-capture", action="store_true", help=argparse.SUPPRESS)
    args = ap.parse_args()

    cfg = Config()
    cfg.swipe_fire_speed /= args.sensitivity
    cfg.swipe_release_speed /= args.sensitivity
    cfg.finger_threshold /= args.sensitivity
    cfg.swipe_distance /= args.sensitivity
    cfg.drop_distance /= args.sensitivity
    cfg.vertical_swipes = not args.no_vertical
    detector = GestureDetector(cfg, args.mode)

    source = int(args.camera) if args.camera.isdigit() else args.camera
    cap = open_camera(source, args.width, args.height, args.backend)
    if not isinstance(source, int):
        frames = SequentialReader(cap, time, video_fps(cap))  # a video file
    elif args.sync_capture:
        frames = SequentialReader(cap, time)
    else:
        print("Camera:", describe_camera(cap, args.backend))
        frames = LatestFrameReader(cap, time)

    landmarker = vision.HandLandmarker.create_from_options(
        vision.HandLandmarkerOptions(
            base_options=BaseOptions(model_asset_path=ensure_model()),
            running_mode=vision.RunningMode.VIDEO,
            num_hands=1,
            min_hand_detection_confidence=0.6,
            min_hand_presence_confidence=0.5,
            min_tracking_confidence=0.5))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    target = ("127.0.0.1", args.port)
    print(f"Sending gestures to udp://127.0.0.1:{args.port} ({detector.mode} mode)")

    stats = Stats() if args.stats else None
    recorder = Recorder(args.record, detector) if args.record else None
    first_stamp = None
    last_now = 0.0
    last_keepalive = 0.0
    last_preview = -1.0
    last_ts_ms = -1
    fps, flash = 0.0, None
    try:
        while True:
            got = frames.read()
            if got is None:
                break
            frame, stamp = got
            # Frame times come from the high-resolution clock: on Windows
            # time.monotonic() only ticks every 15.6 ms, which is half a
            # frame and would spoil every speed the detector measures
            if first_stamp is None:
                first_stamp = stamp
            now = stamp - first_stamp
            # Mirror the image so moving your hand right moves the piece right
            frame = cv2.flip(frame, 1)
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            ts_ms = max(round(now * 1000), last_ts_ms + 1)
            last_ts_ms = ts_ms
            tracking_start = time.perf_counter()
            result = landmarker.detect_for_video(
                mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb), ts_ms)
            tracking = time.perf_counter() - tracking_start

            hand = extract_hand(result)
            landmarks = [(x, y) for x, y, _ in hand["image"]] if hand else None

            commands = detector.update(landmarks, now)
            if commands:
                sock.sendto("".join(commands).encode(), target)
                for c in commands:
                    if c in LABELS:
                        print(LABELS[c], flush=True)
                        flash = (LABELS[c], time.perf_counter())
            elif now - last_keepalive > 0.5:
                sock.sendto(b"K", target)  # tells the game we're alive
                last_keepalive = now

            if recorder:
                recorder.frame(now, hand, commands)
            if stats:
                line = stats.frame(now, tracking, hand is not None, frames.dropped)
                if line:
                    print(line, flush=True)

            dt, last_now = now - last_now, now
            fps = 0.9 * fps + 0.1 * (1 / dt if dt > 0 else 0)
            # Drawing costs time, so the preview is redrawn less often than
            # the camera delivers frames; the gestures use every frame
            if not args.no_preview and now - last_preview >= PREVIEW_INTERVAL:
                last_preview = now
                draw_preview(frame, landmarks, detector, fps, flash)
                cv2.imshow("Tetris++ hand control", frame)
                key = cv2.waitKey(1) & 0xFF
                if key in (ord("q"), 27):
                    break
                if key == ord("m"):
                    detector.toggle_mode()
                    print(f"Switched to {detector.mode} mode")
    except KeyboardInterrupt:
        pass
    finally:
        frames.close()
        cap.release()
        cv2.destroyAllWindows()
        landmarker.close()
        sock.close()
        if recorder:
            recorder.close()
            print(f"Recording saved to {args.record}")


if __name__ == "__main__":
    main()
