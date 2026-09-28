"""Tests for hand_control.py (the webcam -> UDP program).

OpenCV and MediaPipe are replaced by small fakes, so these tests need
neither the packages nor a camera. They feed scripted hand positions through
the real main loop and check the UDP messages that come out.

Run with:  python3 -m unittest gesture/test_hand_control.py   (from the repo root)
"""
import io
import os
import socket
import sys
import tempfile
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
    for name in ("CAP_PROP_FRAME_WIDTH", "CAP_PROP_FRAME_HEIGHT", "CAP_PROP_FPS",
                 "CAP_PROP_BUFFERSIZE", "COLOR_BGR2RGB", "FONT_HERSHEY_SIMPLEX"):
        setattr(cv2, name, 0)
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
from test_gestures import FPS, fist, move, open_hand, still  # noqa: E402


class FakeClock:
    """Replaces the time module inside hand_control: every camera frame
    advances the clock by one frame at 30 fps."""

    def __init__(self):
        self.t = 100.0

    def monotonic(self):
        return self.t

    def time(self):
        return self.t


class FakeCamera:
    def __init__(self, frames, clock):
        self.frames = list(frames)
        self.clock = clock
        self.released = False

    def isOpened(self):
        return True

    def set(self, prop, value):
        pass

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
            return SimpleNamespace(hand_landmarks=[])
        return SimpleNamespace(hand_landmarks=[[SimpleNamespace(x=x, y=y) for x, y in lm]])

    def close(self):
        self.closed = True


def run_main(frames, *args):
    """Runs hand_control.main() on scripted frames and returns the UDP
    datagrams it sent, plus the fakes for further checks."""
    receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    receiver.bind(("127.0.0.1", 0))
    receiver.settimeout(0.2)
    port = receiver.getsockname()[1]

    clock = FakeClock()
    camera = FakeCamera(frames, clock)
    landmarker = FakeLandmarker(frames)
    cv2_fake.VideoCapture = lambda source: camera
    vision_fake.HandLandmarker.create_from_options = lambda options: landmarker

    argv = ["hand_control.py", "--no-preview", "--port", str(port), *args]
    out = io.StringIO()
    with mock.patch.object(sys, "argv", argv), \
            mock.patch.object(hand_control, "time", clock), \
            mock.patch.object(hand_control, "ensure_model", lambda: "model.task"), \
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
        messages, camera, landmarker, out = run_main(frames)
        self.assertIn("R", messages)
        self.assertNotIn("L", messages)
        self.assertIn("RIGHT", out)
        self.assertTrue(camera.released and landmarker.closed)

    def test_every_gesture_reaches_the_game(self):
        frames = (still(0.3, 0.5, 0.3) + move(0.3, 0.7, 0.5, 0.5, 0.2) + still(0.7, 0.5, 0.4)
                  + move(0.7, 0.3, 0.5, 0.5, 0.2) + still(0.3, 0.5, 0.6)
                  + move(0.3, 0.3, 0.5, 0.85, 0.2) + still(0.3, 0.85, 0.4)
                  + still(0.3, 0.85, 1.0, fist))
        messages, *_ = run_main(frames)
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
        normal, *_ = run_main(frames)
        sensitive, *_ = run_main(frames, "--sensitivity", "2")
        self.assertNotIn("R", normal)
        self.assertIn("R", sensitive)

    def test_no_vertical(self):
        frames = still(0.5, 0.3, 0.3) + move(0.5, 0.5, 0.3, 0.7, 0.2) + still(0.5, 0.7, 0.3)
        messages, *_ = run_main(frames, "--no-vertical")
        self.assertNotIn("H", messages)

    def test_timestamps_always_increase(self):
        # MediaPipe's VIDEO mode rejects timestamps that don't increase
        _, _, landmarker, _ = run_main(still(0.5, 0.5, 1.0))
        stamps = landmarker.timestamps
        self.assertTrue(all(b > a for a, b in zip(stamps, stamps[1:])))


class CameraTests(unittest.TestCase):
    def camera_factory(self, working):
        opened = []

        class Cam:
            def __init__(self, source):
                opened.append(source)
                self.ok = source in working

            def isOpened(self):
                return self.ok

            def set(self, *a):
                pass

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
        Cam, _ = self.camera_factory(set())
        cv2_fake.VideoCapture = Cam
        with mock.patch.object(hand_control, "running_in_wsl", lambda: False), \
                redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as err:
            hand_control.open_camera(0, 640, 360)
        self.assertIn("No webcam found", str(err.exception.code))
        self.assertNotIn("WSL", str(err.exception.code))

    def test_wsl_help_when_no_camera(self):
        Cam, _ = self.camera_factory(set())
        cv2_fake.VideoCapture = Cam
        with mock.patch.object(hand_control, "running_in_wsl", lambda: True), \
                redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as err:
            hand_control.open_camera(0, 640, 360)
        message = str(err.exception.code)
        self.assertIn("networkingMode=mirrored", message)
        self.assertIn("--gesture-bind 0.0.0.0", message)
        self.assertIn("run_windows.bat", message)

    def test_missing_video_file_gets_plain_help(self):
        Cam, _ = self.camera_factory(set())
        cv2_fake.VideoCapture = Cam
        with mock.patch.object(hand_control, "running_in_wsl", lambda: True), \
                redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as err:
            hand_control.open_camera("clip.mp4", 640, 360)
        self.assertNotIn("mirrored", str(err.exception.code))

    def test_wsl_detection(self):
        with mock.patch("builtins.open", mock.mock_open(
                read_data="Linux version 6.6.87.2-microsoft-standard-WSL2")):
            self.assertTrue(hand_control.running_in_wsl())
        with mock.patch("builtins.open", mock.mock_open(read_data="Linux version 6.8.0-generic")):
            self.assertFalse(hand_control.running_in_wsl())
        with mock.patch("builtins.open", side_effect=OSError):
            self.assertFalse(hand_control.running_in_wsl())


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
        self.assertEqual(set(hand_control.LABELS), {"L", "R", "H", "U", "C"})

    def test_preview_draws_without_errors(self):
        frame = SimpleNamespace(shape=(360, 640, 3))
        detector = hand_control.GestureDetector(mode="position")
        detector.update(open_hand(0.5, 0.5), 0)
        hand_control.draw_preview(frame, open_hand(0.5, 0.5), detector, 30.0, ("LEFT", 0))
        hand_control.draw_preview(frame, None, detector, 0.0, None)


if __name__ == "__main__":
    unittest.main()
