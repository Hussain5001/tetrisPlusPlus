"""Tests for hand_control.py (the webcam -> UDP program).

OpenCV and MediaPipe are replaced by small fakes, so these tests need
neither the packages nor a camera. They feed scripted hand positions through
the real main loop and check the UDP messages that come out.

Run with:  py -m unittest gesture/test_hand_control.py   (from the repo root)
"""
import io
import os
import socket
import sys
import tempfile
import threading
import time as real_time
import types
import unittest
from contextlib import redirect_stderr, redirect_stdout
from types import SimpleNamespace
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


# ---------------------------------------------------------------- fakes

def install_fakes():
    cv2 = types.ModuleType("cv2")
    for number, name in enumerate(("CAP_PROP_FRAME_WIDTH", "CAP_PROP_FRAME_HEIGHT",
                                   "CAP_PROP_FPS", "CAP_PROP_BUFFERSIZE", "CAP_PROP_FOURCC",
                                   "COLOR_BGR2RGB", "FONT_HERSHEY_SIMPLEX")):
        setattr(cv2, name, number)
    cv2.CAP_DSHOW, cv2.CAP_MSMF = 700, 1400
    cv2.VideoWriter_fourcc = lambda *letters: "".join(letters)
    cv2.VideoCapture = None  # set by each test
    cv2.flip = lambda frame, code: frame
    cv2.cvtColor = lambda frame, code: frame
    for name in ("imshow", "destroyAllWindows", "line", "circle", "rectangle", "putText"):
        setattr(cv2, name, lambda *a, **k: None)
    cv2.waitKey = lambda delay: -1

    mp = types.ModuleType("mediapipe")
    mp.Image = lambda image_format, data: data
    mp.ImageFormat = SimpleNamespace(SRGB=1)
    tasks = types.ModuleType("mediapipe.tasks")
    python = types.ModuleType("mediapipe.tasks.python")
    python.BaseOptions = lambda model_asset_path: SimpleNamespace(path=model_asset_path)
    vision = types.ModuleType("mediapipe.tasks.python.vision")
    vision.HandLandmarkerOptions = lambda **kw: SimpleNamespace(**kw)
    vision.RunningMode = SimpleNamespace(VIDEO="video")
    vision.HandLandmarker = SimpleNamespace(create_from_options=None)  # set by tests
    python.vision = vision
    mp.tasks = tasks
    tasks.python = python
    sys.modules.update({"cv2": cv2, "mediapipe": mp, "mediapipe.tasks": tasks,
                        "mediapipe.tasks.python": python,
                        "mediapipe.tasks.python.vision": vision})
    return cv2, vision


cv2_fake, vision_fake = install_fakes()
import hand_control  # noqa: E402
from test_gestures import (FLICK, FPS, curl_to, fist, hold, move, open_hand,  # noqa: E402
                           pointing, rest, still, swipe, tilt)


class FakeClock:
    """Replaces the time module inside hand_control: every camera frame
    advances the clock by one frame at 30 fps."""

    def __init__(self):
        self.t = 100.0

    def perf_counter(self):
        return self.t

    def monotonic(self):
        # Like Windows before Python 3.13: this clock only ticks 64 times a
        # second, too coarse to time camera frames with
        return int(self.t * 64) / 64

    def time(self):
        return self.monotonic()


class FakeCamera:
    def __init__(self, frames, clock, fps=30.0):
        self.frames = list(frames)
        self.clock = clock
        self.released = False
        self.settings = {}
        self.fps = fps

    def isOpened(self):
        return True

    def set(self, prop, value):
        self.settings[prop] = value

    def get(self, prop):
        return {cv2_fake.CAP_PROP_FRAME_WIDTH: 640, cv2_fake.CAP_PROP_FRAME_HEIGHT: 360,
                cv2_fake.CAP_PROP_FPS: self.fps}.get(prop, 0)

    def read(self):
        if not self.frames:
            return False, None
        self.clock.t += 1 / FPS
        return True, self.frames.pop(0)

    def release(self):
        self.released = True


class FakeLandmarker:
    def __init__(self, frames):
        self.frames = frames
        self.closed = False
        self.timestamps = []

    def detect_for_video(self, image, ts_ms):
        self.timestamps.append(ts_ms)
        lm = image  # the fake camera "frame" is the landmark list itself
        if lm is None:
            return SimpleNamespace(hand_landmarks=[], hand_world_landmarks=[], handedness=[])
        return SimpleNamespace(
            hand_landmarks=[[SimpleNamespace(x=x, y=y, z=0.0) for x, y in lm]],
            hand_world_landmarks=[[SimpleNamespace(x=x - 0.5, y=y - 0.5, z=0.01)
                                   for x, y in lm]],
            handedness=[[SimpleNamespace(category_name="Right", score=0.97)]])

    def close(self):
        self.closed = True


def run_main(frames, *args, preview=False, camera_fps=30.0):
    """Runs hand_control.main() on scripted frames and returns the UDP
    datagrams it sent, plus the fakes for further checks. The frames are
    read one by one (--sync-capture), so every run gives the same result."""
    receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    receiver.bind(("127.0.0.1", 0))
    receiver.settimeout(0.2)
    port = receiver.getsockname()[1]

    clock = FakeClock()
    camera = FakeCamera(frames, clock, camera_fps)
    landmarker = FakeLandmarker(frames)
    cv2_fake.VideoCapture = lambda source, backend=None: camera
    vision_fake.HandLandmarker.create_from_options = lambda options: landmarker

    argv = ["hand_control.py", "--sync-capture", "--port", str(port), *args]
    if not preview:
        argv.append("--no-preview")
    out = io.StringIO()
    # The fake frames are landmark lists, not pictures, so nothing is drawn
    # on them here (SmallPieceTests draws the preview)
    with mock.patch.object(sys, "argv", argv), \
            mock.patch.object(hand_control, "time", clock), \
            mock.patch.object(hand_control, "ensure_model", lambda: "model.task"), \
            mock.patch.object(hand_control, "draw_preview", lambda *a: None), \
            redirect_stdout(out):
        hand_control.main()

    messages = []
    try:
        while True:
            messages.append(receiver.recv(64).decode())
    except socket.timeout:
        pass
    receiver.close()
    return messages, camera, landmarker, out.getvalue()


# ---------------------------------------------------------------- tests

class MainLoopTests(unittest.TestCase):
    def test_swipe_right_is_sent(self):
        frames = still(0.3, 0.5, 0.3) + move(0.3, 0.7, 0.5, 0.5, 0.2) + still(0.7, 0.5, 0.3)
        messages, camera, landmarker, out = run_main(frames, "--mode", "palm")
        self.assertIn("R", messages)
        self.assertNotIn("L", messages)
        self.assertIn("RIGHT", out)
        self.assertTrue(camera.released and landmarker.closed)

    def test_every_gesture_reaches_the_game(self):
        frames = (still(0.3, 0.5, 0.3) + move(0.3, 0.7, 0.5, 0.5, 0.2) + still(0.7, 0.5, 0.4)
                  + move(0.7, 0.3, 0.5, 0.5, 0.2) + still(0.3, 0.5, 0.6)
                  + move(0.3, 0.3, 0.5, 0.85, 0.2) + still(0.3, 0.85, 0.4)
                  + still(0.3, 0.85, 1.0, fist))
        messages, *_ = run_main(frames, "--mode", "palm")
        commands = [m for m in messages if m != "K"]
        self.assertEqual(commands, ["R", "L", "H", "C"])

    def test_keepalive_without_a_hand(self):
        messages, *_ = run_main([None] * 60)  # two seconds, nobody there
        self.assertGreaterEqual(messages.count("K"), 2)
        self.assertEqual(set(messages), {"K"})

    def test_position_mode_sends_columns(self):
        frames = still(0.2, 0.5, 0.2) + move(0.2, 0.79, 0.5, 0.5, 0.5)
        messages, *_ = run_main(frames, "--mode", "position")
        columns = [int(m[1:]) for m in messages if m.startswith("@")]
        self.assertEqual(columns[0], 0)
        self.assertEqual(columns[-1], 9)
        self.assertEqual(columns, sorted(columns))
        self.assertNotIn("R", messages)

    def test_sensitivity(self):
        # A small, slowish swipe: ignored normally, recognised when sensitive
        frames = still(0.4, 0.5, 0.3) + move(0.4, 0.5, 0.5, 0.5, 0.25) + still(0.5, 0.5, 0.3)
        normal, *_ = run_main(frames, "--mode", "palm")
        sensitive, *_ = run_main(frames, "--mode", "palm", "--sensitivity", "2")
        self.assertNotIn("R", normal)
        self.assertIn("R", sensitive)

    def test_no_vertical(self):
        frames = still(0.5, 0.3, 0.3) + move(0.5, 0.5, 0.3, 0.7, 0.2) + still(0.5, 0.7, 0.3)
        messages, *_ = run_main(frames, "--mode", "palm", "--no-vertical")
        self.assertNotIn("H", messages)

    def test_finger_swipes_are_the_default(self):
        frames = (rest(0.5, 0.4) + swipe(0.5, 0.4) + swipe(0.4, 0.5, 0.3) + rest(0.5, 0.3)
                  + swipe(0.5, 0.6) + swipe(0.6, 0.5, 0.3) + rest(0.5, 0.3)
                  + curl_to(0, 0.6, 0.1) + curl_to(0.6, 0, 0.1) + hold(0, 0.4)
                  + still(0.5, 0.5, 1.5, fist)                      # resting: nothing
                  + still(0.5, 0.5, 1.5))                           # open palm: pause
        messages, _, _, out = run_main(frames)
        commands = [m for m in messages if m != "K"]
        self.assertEqual(commands, ["L", "R", "H", "P"])
        self.assertIn("(flick mode)", out)
        self.assertIn("PAUSE", out)

    def test_point_mode_repeats_while_held(self):
        frames = hold(0, 0.3) + tilt(0, FLICK, 0.05) + hold(FLICK, 1.0) + tilt(FLICK, 0, 0.05)
        messages, *_ = run_main(frames, "--mode", "point")
        self.assertGreaterEqual(messages.count("R"), 4)
        self.assertNotIn("L", messages)

    def test_finger_sensitivity(self):
        # A small ~17 degree flick: ignored normally, recognised when sensitive
        frames = hold(0, 0.4) + tilt(0, 0.3, 0.1) + hold(0.3, 0.3)
        normal, *_ = run_main(frames)
        sensitive, *_ = run_main(frames, "--sensitivity", "2")
        self.assertNotIn("R", normal)
        self.assertIn("R", sensitive)

    def test_old_swipe_mode_name_still_works(self):
        frames = still(0.3, 0.5, 0.3) + move(0.3, 0.7, 0.5, 0.5, 0.2) + still(0.7, 0.5, 0.3)
        messages, _, _, out = run_main(frames, "--mode", "swipe")
        self.assertIn("R", messages)
        self.assertIn("(palm mode)", out)

    def test_timestamps_always_increase(self):
        # MediaPipe's VIDEO mode rejects timestamps that don't increase
        _, _, landmarker, _ = run_main(still(0.5, 0.5, 1.0))
        stamps = landmarker.timestamps
        self.assertTrue(all(b > a for a, b in zip(stamps, stamps[1:])))

    def test_frames_are_timed_with_the_precise_clock(self):
        # time.monotonic() ticks every 15.6 ms on Windows: with it, frames
        # 33 ms apart would look 31 or 47 ms apart and every speed is wrong
        _, _, landmarker, _ = run_main(still(0.5, 0.5, 2.0))
        gaps = {b - a for a, b in zip(landmarker.timestamps, landmarker.timestamps[1:])}
        self.assertLessEqual(gaps, {33, 34})

    def test_video_files_are_timed_by_their_own_frame_rate(self):
        # a 25 fps clip gives the same times on every run, whatever the clock says
        _, _, landmarker, _ = run_main(still(0.5, 0.5, 1.0), "--camera", "clip.mp4",
                                       camera_fps=25.0)
        gaps = {b - a for a, b in zip(landmarker.timestamps, landmarker.timestamps[1:])}
        self.assertEqual(gaps, {40})

    def test_stats_are_printed(self):
        frames = rest(0.5, 3.0) + [None] * 3 + rest(0.5, 3.0)
        _, _, _, out = run_main(frames, "--stats")
        lines = [line for line in out.splitlines() if line.startswith("stats:")]
        self.assertEqual(len(lines), 1)            # one line per 5 seconds
        self.assertIn("stats: 30.0 fps", lines[0])
        self.assertIn("lost 1 times", lines[0])
        self.assertNotIn("stats:", run_main(frames)[3])

    def test_preview_is_drawn_less_often_than_frames_arrive(self):
        shown = []
        with mock.patch.object(cv2_fake, "imshow", lambda *a: shown.append(1)):
            run_main(still(0.5, 0.5, 2.0), preview=True)      # 60 frames
        self.assertEqual(len(shown), 30)                      # every second frame

    def test_preview_keys_still_work(self):
        keys = iter([ord("m")] + [-1] * 5 + [ord("q")])
        with mock.patch.object(cv2_fake, "waitKey", lambda delay: next(keys, -1)):
            _, camera, _, out = run_main(still(0.5, 0.5, 3.0), preview=True)
        self.assertIn("Switched to point mode", out)
        self.assertTrue(camera.frames)                        # q stopped it early


class CameraTests(unittest.TestCase):
    def camera_factory(self, working):
        opened = []

        class Cam:
            def __init__(self, source, backend=None):
                opened.append(source)
                self.ok = source in working
                self.backend = backend
                self.settings = {}

            def isOpened(self):
                return self.ok

            def set(self, prop, value):
                self.settings[prop] = value

            def get(self, prop):
                return {cv2_fake.CAP_PROP_FRAME_WIDTH: 640, cv2_fake.CAP_PROP_FRAME_HEIGHT: 480,
                        cv2_fake.CAP_PROP_FPS: 30.0}.get(prop, 0)

            def release(self):
                pass

        return Cam, opened

    def test_tries_other_camera_indices(self):
        Cam, opened = self.camera_factory({2})
        cv2_fake.VideoCapture = Cam
        with redirect_stdout(io.StringIO()) as out:
            cap = hand_control.open_camera(0, 640, 360)
        self.assertTrue(cap.ok)
        self.assertEqual(opened, [0, 1, 2])
        self.assertIn("Using camera 2", out.getvalue())

    def test_explicit_camera_is_not_replaced(self):
        Cam, opened = self.camera_factory({0})
        cv2_fake.VideoCapture = Cam
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            hand_control.open_camera(1, 640, 360)
        self.assertEqual(opened, [1])

    def test_help_when_no_camera(self):
        Cam, opened = self.camera_factory(set())
        cv2_fake.VideoCapture = Cam
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as err:
            hand_control.open_camera(0, 640, 360)
        self.assertIn("No webcam found", str(err.exception.code))
        self.assertEqual(opened, [0, 1, 2, 3])

    def test_missing_video_file_gets_help(self):
        Cam, opened = self.camera_factory(set())
        cv2_fake.VideoCapture = Cam
        with redirect_stderr(io.StringIO()) as shown, self.assertRaises(SystemExit) as err:
            hand_control.open_camera("clip.mp4", 640, 360)
        self.assertIn("No webcam found", str(err.exception.code))
        self.assertIn("clip.mp4", shown.getvalue())
        self.assertEqual(opened, ["clip.mp4"])


class BackendTests(unittest.TestCase):
    def open(self, backend):
        made = []

        class Cam:
            def __init__(self, *args):
                made.append(args)
                self.settings = {}

            def isOpened(self):
                return True

            def set(self, prop, value):
                self.settings[prop] = value

            def get(self, prop):
                return {cv2_fake.CAP_PROP_FRAME_WIDTH: 640, cv2_fake.CAP_PROP_FRAME_HEIGHT: 480,
                        cv2_fake.CAP_PROP_FPS: 30.0}.get(prop, 0)

        cv2_fake.VideoCapture = Cam
        return hand_control.open_camera(0, 640, 360, backend), made

    def test_default_backend_is_left_to_opencv(self):
        cap, made = self.open("auto")
        self.assertEqual(made, [(0,)])
        self.assertNotIn(cv2_fake.CAP_PROP_FOURCC, cap.settings)

    def test_directshow_asks_for_compressed_frames(self):
        cap, made = self.open("dshow")
        self.assertEqual(made, [(0, cv2_fake.CAP_DSHOW)])
        self.assertEqual(cap.settings[cv2_fake.CAP_PROP_FOURCC], "MJPG")
        cap, made = self.open("msmf")
        self.assertEqual(made, [(0, cv2_fake.CAP_MSMF)])
        self.assertNotIn(cv2_fake.CAP_PROP_FOURCC, cap.settings)

    def test_camera_description_shows_what_it_really_delivers(self):
        cap, _ = self.open("dshow")
        self.assertEqual(hand_control.describe_camera(cap, "dshow"),
                         "640x480, camera reports 30 fps (dshow backend)")


class SlowCamera:
    """A camera that delivers numbered frames every few milliseconds."""

    def __init__(self, count, interval=0.003):
        self.count, self.interval, self.sent = count, interval, 0

    def read(self):
        if self.sent >= self.count:
            return False, None
        real_time.sleep(self.interval)
        self.sent += 1
        return True, self.sent - 1


class SteppedCamera:
    """A camera that delivers its next numbered frame only when the test
    says so, which makes the order of events certain."""

    def __init__(self, count):
        self.count, self.sent = count, 0
        self.next_frame = threading.Semaphore(0)

    def read(self):
        if self.sent >= self.count:
            return False, None
        self.next_frame.acquire()
        self.sent += 1
        return True, self.sent - 1


class FrameReaderTests(unittest.TestCase):
    def test_a_slow_reader_gets_the_newest_frame(self):
        reader = hand_control.LatestFrameReader(SlowCamera(60), real_time)
        seen, stamps = [], []
        while True:
            got = reader.read()
            if got is None:
                break
            seen.append(got[0])
            stamps.append(got[1])
            real_time.sleep(0.02)          # tracking is slower than the camera
        reader.close()
        self.assertEqual(seen, sorted(set(seen)))           # in order, none twice
        self.assertEqual(seen[-1], 59)                      # the last frame is never lost
        self.assertGreater(reader.dropped, 0)
        self.assertEqual(len(seen) + reader.dropped, 60)    # every frame used or skipped
        self.assertEqual(stamps, sorted(stamps))

    def test_a_reader_that_keeps_up_gets_every_frame(self):
        camera = SteppedCamera(20)
        reader = hand_control.LatestFrameReader(camera, real_time)
        seen = []
        for _ in range(20):
            camera.next_frame.release()    # the camera delivers one frame...
            seen.append(reader.read()[0])  # ...and it is read before the next one
        self.assertIsNone(reader.read())   # the camera has stopped
        reader.close()
        self.assertEqual(seen, list(range(20)))
        self.assertEqual(reader.dropped, 0)

    def test_close_stops_reading(self):
        camera = SlowCamera(100000)
        reader = hand_control.LatestFrameReader(camera, real_time)
        reader.read()
        reader.close()
        sent = camera.sent
        real_time.sleep(0.05)
        self.assertEqual(camera.sent, sent)

    def test_video_files_give_every_frame_with_file_times(self):
        clock = FakeClock()
        reader = hand_control.SequentialReader(FakeCamera(["a", "b", "c"], clock), clock, fps=25.0)
        got = [reader.read() for _ in range(4)]
        self.assertEqual(got[:3], [("a", 0.0), ("b", 0.04), ("c", 0.08)])
        self.assertIsNone(got[3])
        self.assertEqual(reader.dropped, 0)

    def test_video_frame_rate_falls_back_to_30(self):
        clock = FakeClock()
        self.assertEqual(hand_control.video_fps(FakeCamera([], clock, fps=25.0)), 25.0)
        self.assertEqual(hand_control.video_fps(FakeCamera([], clock, fps=0)), 30.0)
        self.assertEqual(hand_control.video_fps(FakeCamera([], clock, fps=float("nan"))), 30.0)


class StatsTests(unittest.TestCase):
    def test_one_line_per_period(self):
        stats = hand_control.Stats(period=1.0)
        lines = []
        for i in range(61):                       # two seconds at 30 fps
            seen = not 10 <= i < 13               # the hand is lost once, for 3 frames
            dropped = 2 if i >= 20 else 0
            line = stats.frame(i / 30, 0.012 if i != 5 else 0.030, seen, dropped)
            if line:
                lines.append(line)
        self.assertEqual(len(lines), 2)
        self.assertIn("stats: 30.0 fps, longest gap 33 ms", lines[0])
        self.assertIn("tracking 12.6 ms avg, 30 ms worst", lines[0])
        self.assertIn("2 stale frames skipped", lines[0])
        self.assertIn("hand seen 90% of frames, lost 1 times", lines[0])
        # the second period starts fresh
        self.assertIn("tracking 12.0 ms avg, 12 ms worst", lines[1])
        self.assertIn("0 stale frames skipped", lines[1])
        self.assertIn("hand seen 100% of frames, lost 0 times", lines[1])

    def test_a_long_gap_shows_up(self):
        stats = hand_control.Stats(period=1.0)
        times = [0, 0.033, 0.066, 0.4, 0.433, 1.01]
        lines = [stats.frame(t, 0.01, True, 0) for t in times]
        self.assertIn("longest gap 577 ms", lines[-1])


class HandDataTests(unittest.TestCase):
    def test_no_hand(self):
        self.assertIsNone(hand_control.extract_hand(SimpleNamespace(hand_landmarks=[])))

    def test_landmarks_world_and_side(self):
        result = FakeLandmarker([]).detect_for_video(open_hand(0.5, 0.5), 0)
        hand = hand_control.extract_hand(result)
        self.assertEqual(len(hand["image"]), 21)
        self.assertEqual(len(hand["image"][0]), 3)
        self.assertEqual(len(hand["world"]), 21)
        self.assertAlmostEqual(hand["world"][0][2], 0.01)
        self.assertEqual((hand["side"], hand["score"]), ("Right", 0.97))

    def test_a_result_without_the_extras_still_works(self):
        result = SimpleNamespace(hand_landmarks=[[SimpleNamespace(x=0.1, y=0.2)] * 21])
        hand = hand_control.extract_hand(result)
        self.assertEqual(hand["image"][0], (0.1, 0.2, 0.0))
        self.assertIsNone(hand["world"])
        self.assertEqual(hand["side"], "")


class ModelTests(unittest.TestCase):
    def test_model_downloaded_once(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "models", "hand.task")
            downloads = []

            def fake_download(url, target):
                downloads.append(url)
                with open(target, "wb") as f:
                    f.write(b"model")

            with mock.patch.object(hand_control, "MODEL_PATH", path), \
                    mock.patch.object(hand_control.urllib.request, "urlretrieve", fake_download), \
                    redirect_stdout(io.StringIO()):
                first = hand_control.ensure_model()
                second = hand_control.ensure_model()
            self.assertEqual(first, path)
            self.assertEqual(second, path)
            self.assertEqual(len(downloads), 1)
            self.assertTrue(downloads[0].startswith("https://"))


class SmallPieceTests(unittest.TestCase):
    def test_labels_cover_all_commands(self):
        self.assertEqual(set(hand_control.LABELS), {"L", "R", "H", "U", "C", "P"})

    def test_preview_draws_without_errors(self):
        frame = SimpleNamespace(shape=(360, 640, 3))
        detector = hand_control.GestureDetector(mode="position")
        detector.update(open_hand(0.5, 0.5), 0)
        hand_control.draw_preview(frame, open_hand(0.5, 0.5), detector, 30.0, ("LEFT", 0))
        hand_control.draw_preview(frame, None, detector, 0.0, None)
        finger = hand_control.GestureDetector(mode="flick")
        for i, lm in enumerate(hold(0, 0.2) + hold(FLICK, 0.1)):
            finger.update(lm, i / FPS)
        self.assertTrue(finger.debug["pointing"])
        hand_control.draw_preview(frame, pointing(0.5, 0.5, FLICK), finger, 30.0, ("RIGHT", 0))
        finger.debug["pointing"] = False
        hand_control.draw_preview(frame, None, finger, 30.0, None)


if __name__ == "__main__":
    unittest.main()
