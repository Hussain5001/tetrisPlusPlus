"""Tests for recordings (hand_control.py --record) and replay.py.

A recording is made by running the real camera loop on scripted hand
movements (with the fakes from test_hand_control), then replayed.

Run with:  py -m unittest gesture/test_replay.py   (from the repo root)
"""
import io
import json
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import replay  # noqa: E402
from gestures import GestureDetector  # noqa: E402
from test_gestures import curl_to, hold, open_hand, rest, swipe  # noqa: E402
from test_hand_control import run_main  # noqa: E402

# left, right, a drop, then the hand leaves the picture
SESSION = (rest(0.5, 0.4) + swipe(0.5, 0.4) + swipe(0.4, 0.5, 0.3) + rest(0.5, 0.3)
           + swipe(0.5, 0.6) + swipe(0.6, 0.5, 0.3) + rest(0.5, 0.3)
           + curl_to(0, 0.6, 0.1) + curl_to(0.6, 0, 0.1) + hold(0, 0.4) + [None] * 6)


class RecordingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.path = os.path.join(cls.tmp.name, "session.jsonl")
        cls.messages, _, _, cls.out = run_main(SESSION, "--record", cls.path)
        cls.header, cls.frames = replay.load(cls.path)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_recording_has_a_header_and_every_frame(self):
        self.assertEqual(self.header["mode"], "flick")
        self.assertEqual(self.header["config"]["swipe_distance"], 0.35)
        self.assertEqual(len(self.frames), len(SESSION))
        self.assertIn("Recording saved to", self.out)

    def test_frames_hold_landmarks_and_commands(self):
        first, last = self.frames[0], self.frames[-1]
        self.assertEqual((len(first["lm"]), len(first["lm"][0])), (21, 3))
        self.assertEqual(len(first["world"]), 21)
        self.assertEqual(first["side"], "Right")
        self.assertEqual(first["t"], 0)
        self.assertIsNone(last["lm"])                      # no hand in the picture
        times = [f["t"] for f in self.frames]
        self.assertEqual(times, sorted(times))
        self.assertEqual(replay.recorded_commands(self.frames), ["L", "R", "H"])
        self.assertEqual([m for m in self.messages if m != "K"], ["L", "R", "H"])

    def test_replay_gives_the_recorded_commands(self):
        detector = GestureDetector(replay.make_config(self.header), self.header["mode"])
        events = replay.replay(self.frames, detector)
        self.assertEqual([c for _, c in events], ["L", "R", "H"])
        self.assertEqual([t for t, _ in events], sorted(t for t, _ in events))

    def test_settings_can_be_changed_for_a_replay(self):
        cfg = replay.make_config(self.header, ["swipe_distance=9", "vertical_swipes=no"])
        self.assertEqual((cfg.swipe_distance, cfg.vertical_swipes), (9.0, False))
        events = replay.replay(self.frames, GestureDetector(cfg, "flick"))
        self.assertEqual(events, [])                       # nothing is far enough now
        cfg = replay.make_config(self.header, ["max_gap_frames=2"])
        self.assertEqual(cfg.max_gap_frames, 2)
        with self.assertRaises(ValueError):
            replay.make_config(self.header, ["no_such_setting=1"])
        with self.assertRaises(ValueError):
            replay.make_config(self.header, ["swipe_distance"])

    def test_settings_of_an_older_recording_are_ignored_when_unknown(self):
        cfg = replay.make_config({"config": {"swipe_distance": 0.5, "long_gone": 3}})
        self.assertEqual(cfg.swipe_distance, 0.5)
        self.assertFalse(hasattr(cfg, "long_gone"))

    def test_report(self):
        events = replay.replay(self.frames, GestureDetector(replay.make_config(self.header)))
        text = replay.report(self.header, self.frames, events, "flick",
                             replay.parse_expect("L=2,H=1,U=0"), timeline=True)
        lines = text.splitlines()
        self.assertEqual(lines[0], f"recording: {len(SESSION)} frames, "
                                   f"{self.frames[-1]['t']:.1f} s, hand seen in "
                                   f"{len(SESSION) - 6} ({100 * (len(SESSION) - 6) / len(SESSION):.0f}%)")
        self.assertEqual(lines[1], "recorded in flick mode, replayed in flick mode")
        self.assertEqual(lines[2], "commands: L 1, R 1, H 1")
        self.assertEqual(lines[3], "same as when it was recorded")
        self.assertEqual(lines[4], "expected L x2: got 1 (missed 1, extra 0)")
        self.assertEqual(lines[5], "expected H x1: got 1 (missed 0, extra 0)")
        self.assertEqual(lines[6], "expected U x0: got 0 (missed 0, extra 0)")
        self.assertEqual(lines[7], "false commands: R 1")
        self.assertEqual(len(lines), 8 + 3)                # the timeline: one line each
        self.assertTrue(lines[8].endswith("L") and lines[10].endswith("H"))

    def test_report_says_when_a_replay_differs(self):
        cfg = replay.make_config(self.header, ["swipe_distance=9"])
        events = replay.replay(self.frames, GestureDetector(cfg, "flick"))
        text = replay.report(self.header, self.frames, events, "flick")
        self.assertIn("commands: H 1", text)
        self.assertIn("differs from the recording, which had: L 1, R 1, H 1", text)

    def test_command_line(self):
        out = io.StringIO()
        with redirect_stdout(out):
            replay.main([self.path, "--expect", "H=1", "--set", "drop_distance=9"])
        self.assertIn("expected H x1: got 0 (missed 1, extra 0)", out.getvalue())
        self.assertIn("false commands: L 1, R 1", out.getvalue())
        out = io.StringIO()
        with redirect_stdout(out):
            replay.main([self.path, "--mode", "point"])
        self.assertIn("recorded in flick mode, replayed in point mode", out.getvalue())

    def test_command_line_errors(self):
        for argv in ([os.path.join(self.tmp.name, "missing.jsonl")],
                     [self.path, "--set", "nonsense=1"],
                     [self.path, "--expect", "rotate=two"]):
            with self.assertRaises(SystemExit) as err:
                replay.main(argv)
            self.assertIn("replay.py:", str(err.exception.code))


class SmallPieceTests(unittest.TestCase):
    def test_expect(self):
        self.assertEqual(replay.parse_expect("U=20, h=0"), {"U": 20, "H": 0})
        self.assertEqual(replay.parse_expect(None), {})
        for bad in ("U", "U=x", "UP=3"):
            with self.assertRaises(ValueError):
                replay.parse_expect(bad)

    def test_score(self):
        result = replay.score({"U": 22, "H": 1, "L": 3}, {"U": 20, "H": 2, "P": 1})
        self.assertEqual(result["per_command"], {"U": (20, 22, 0, 2), "H": (2, 1, 1, 0),
                                                 "P": (1, 0, 1, 0)})
        self.assertEqual(result["false"], {"L": 3})

    def test_not_a_recording(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "other.jsonl")
            with open(path, "w") as f:
                f.write(json.dumps({"something": "else"}) + "\n")
            with self.assertRaises(ValueError):
                replay.load(path)
            open(path, "w").close()
            with self.assertRaises(ValueError):
                replay.load(path)

    def test_position_targets_are_not_commands(self):
        hand = [[x, y, 0] for x, y in open_hand(0.5, 0.5)]
        frames = [{"t": i / 30, "lm": hand, "cmd": ["@5"]} for i in range(5)]
        events = replay.replay(frames, GestureDetector(mode="position"))
        self.assertEqual(events, [])
        self.assertEqual(replay.recorded_commands(frames), [])


if __name__ == "__main__":
    unittest.main()
