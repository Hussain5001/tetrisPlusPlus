#!/usr/bin/env python3
"""Hands-free controller for Tetris++.

Reads the webcam, tracks one hand with MediaPipe and sends commands to the
game over UDP (127.0.0.1:5005). Start the game first, then run:

    python3 gesture/hand_control.py                # flick mode (default)
    python3 gesture/hand_control.py --mode point   # point and hold

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
"""
from __future__ import annotations

import argparse
import os
import socket
import sys
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


def ensure_model():
    if os.path.exists(MODEL_PATH):
        return MODEL_PATH
    os.makedirs(os.path.dirname(MODEL_PATH), exist_ok=True)
    print("Downloading hand model (~8 MB)...")
    urllib.request.urlretrieve(MODEL_URL, MODEL_PATH)
    return MODEL_PATH


def running_in_wsl():
    try:
        with open("/proc/version") as f:
            return "microsoft" in f.read().lower()
    except OSError:
        return False


CAMERA_HELP_WSL = """
No webcam found. You're running inside WSL, and WSL2 can't see the laptop's
camera. Run the hand tracking on Windows instead; the game can stay in WSL:

  1. Easiest (Windows 11): turn on mirrored networking so Windows and WSL
     share localhost. Put this in %UserProfile%\\.wslconfig, then run
     `wsl --shutdown` in PowerShell and reopen WSL:
         [wsl2]
         networkingMode=mirrored
     Start the game in WSL (./build/Tetris), then on Windows double-click
     gesture\\run_windows.bat

  2. Without mirrored networking: start the game with
         ./build/Tetris --gesture-bind 0.0.0.0
     get the WSL address with `hostname -I` (first address), then on Windows:
         gesture\\run_windows.bat --host <that address>

See README.md, "Playing from WSL".
"""

CAMERA_HELP = """
No webcam found. Check that a camera is connected and not used by another
app, or pick one with --camera 1 (or a video file with --camera clip.mp4).
"""


def open_camera(source, width, height):
    # With the default camera, also try the next few indices (USB cams are
    # often 1 or 2)
    candidates = [source] if not isinstance(source, int) or source != 0 else [0, 1, 2, 3]
    for candidate in candidates:
        cap = cv2.VideoCapture(candidate)
        if not cap.isOpened():
            cap.release()
            continue
        # Small frames and a 1-frame buffer keep latency low
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        cap.set(cv2.CAP_PROP_FPS, 60)
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        if candidate != source:
            print(f"Using camera {candidate}")
        return cap
    print(f"Could not open camera/video: {source}", file=sys.stderr)
    sys.exit(CAMERA_HELP_WSL if running_in_wsl() and isinstance(source, int) else CAMERA_HELP)


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
    if flash and time.time() - flash[1] < 0.4:
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


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--camera", default="0", help="camera index or video file")
    ap.add_argument("--mode", choices=list(MODES) + list(MODE_ALIASES), default="flick",
                    help="flick (default), point, palm or position")
    ap.add_argument("--host", default="127.0.0.1",
                    help="address of the game (the WSL IP when the game runs in WSL)")
    ap.add_argument("--port", type=int, default=5005)
    ap.add_argument("--width", type=int, default=640)
    ap.add_argument("--height", type=int, default=360)
    ap.add_argument("--no-preview", action="store_true", help="don't open a window")
    ap.add_argument("--sensitivity", type=float, default=1.0,
                    help=">1 = smaller/slower swipes trigger, <1 = need bigger swipes")
    ap.add_argument("--no-vertical", action="store_true",
                    help="no drop flick / up-down swipes (pinch still rotates, fist pauses)")
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
    cap = open_camera(source, args.width, args.height)

    landmarker = vision.HandLandmarker.create_from_options(
        vision.HandLandmarkerOptions(
            base_options=BaseOptions(model_asset_path=ensure_model()),
            running_mode=vision.RunningMode.VIDEO,
            num_hands=1,
            min_hand_detection_confidence=0.6,
            min_hand_presence_confidence=0.5,
            min_tracking_confidence=0.5))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    target = (args.host, args.port)
    print(f"Sending gestures to udp://{args.host}:{args.port} ({detector.mode} mode)")

    start = time.monotonic()
    last_keepalive = 0.0
    last_ts_ms = -1
    fps, flash = 0.0, None
    prev_loop = time.monotonic()
    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                break
            now = time.monotonic() - start
            # Mirror the image so moving your hand right moves the piece right
            frame = cv2.flip(frame, 1)
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            ts_ms = max(int(now * 1000), last_ts_ms + 1)
            last_ts_ms = ts_ms
            result = landmarker.detect_for_video(
                mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb), ts_ms)

            landmarks = None
            if result.hand_landmarks:
                landmarks = [(p.x, p.y) for p in result.hand_landmarks[0]]

            commands = detector.update(landmarks, now)
            if commands:
                sock.sendto("".join(commands).encode(), target)
                for c in commands:
                    if c in LABELS:
                        print(LABELS[c], flush=True)
                        flash = (LABELS[c], time.time())
            elif now - last_keepalive > 0.5:
                sock.sendto(b"K", target)  # tells the game we're alive
                last_keepalive = now

            loop_now = time.monotonic()
            dt, prev_loop = loop_now - prev_loop, loop_now
            fps = 0.9 * fps + 0.1 * (1 / dt if dt > 0 else 0)
            if not args.no_preview:
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
        cap.release()
        cv2.destroyAllWindows()
        landmarker.close()
        sock.close()


if __name__ == "__main__":
    main()
