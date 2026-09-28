"""Unit tests for the gesture detector using synthetic hand movements.

Run with:  python3 -m unittest gesture/test_gestures.py   (from the repo root)
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gestures import Config, GestureDetector  # noqa: E402

FPS = 30.0


def open_hand(cx, cy, size=0.15):
    """21 landmarks of an open hand with its palm centred near (cx, cy)."""
    s = size
    pts = [(0, 0.5)]                                            # wrist
    pts += [(-0.35, 0.3), (-0.55, 0.1), (-0.7, -0.05), (-0.85, -0.2)]  # thumb
    for dx in (-0.25, -0.05, 0.15, 0.32):                        # 4 fingers
        pts += [(dx, -0.5), (dx, -0.85), (dx, -1.1), (dx, -1.3)]
    # wrist->middle knuckle distance is 1.0 * s
    return [(cx + x * s, cy + 0.2 * s + y * s) for x, y in pts]


def fist(cx, cy, size=0.15):
    pts = open_hand(cx, cy, size)
    wrist = pts[0]
    for mcp in (5, 9, 13, 17):
        # curl: pip slightly out, tip back towards the palm
        mx, my = pts[mcp]
        pts[mcp + 1] = (mx, my - 0.25 * size)
        pts[mcp + 2] = (mx, my - 0.05 * size)
        pts[mcp + 3] = ((mx + wrist[0]) / 2, (my + wrist[1]) / 2)
    return pts


def pinch(cx, cy, size=0.15):
    pts = open_hand(cx, cy, size)
    pts[4] = (pts[8][0] + 0.01, pts[8][1])
    return pts


def run(detector, frames, t0=0.0):
    """frames: list of landmark lists (or None). Returns all commands."""
    out = []
    for i, lm in enumerate(frames):
        out += detector.update(lm, t0 + i / FPS)
    return out


def move(x0, x1, y0, y1, seconds, shape=open_hand):
    n = max(int(seconds * FPS), 1)
    return [shape(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n) for i in range(n + 1)]


def still(x, y, seconds, shape=open_hand):
    return [shape(x, y) for _ in range(int(seconds * FPS))]


class SwipeTests(unittest.TestCase):
    def test_holding_still_sends_nothing(self):
        self.assertEqual(run(GestureDetector(), still(0.5, 0.5, 2)), [])

    def test_slow_drift_sends_nothing(self):
        self.assertEqual(run(GestureDetector(), move(0.3, 0.7, 0.5, 0.5, 3)), [])

    def test_swipe_right_and_left(self):
        d = GestureDetector()
        cmds = run(d, still(0.3, 0.5, 0.3) + move(0.3, 0.7, 0.5, 0.5, 0.2) + still(0.7, 0.5, 0.5))
        self.assertEqual(cmds, ["R"])
        cmds = run(d, move(0.7, 0.3, 0.5, 0.5, 0.2) + still(0.3, 0.5, 0.5), t0=2)
        self.assertEqual(cmds, ["L"])

    def test_one_long_swipe_counts_once(self):
        cmds = run(GestureDetector(), still(0.1, 0.5, 0.3) + move(0.1, 0.9, 0.5, 0.5, 0.5))
        self.assertEqual(cmds, ["R"])

    def test_swipe_fires_early(self):
        # The command should be sent within the first ~100 ms of the movement
        d = GestureDetector()
        frames = still(0.3, 0.5, 0.3) + move(0.3, 0.7, 0.5, 0.5, 0.25)
        fired_at = None
        for i, lm in enumerate(frames):
            if d.update(lm, i / FPS):
                fired_at = i / FPS - 0.3
                break
        self.assertIsNotNone(fired_at)
        self.assertLess(fired_at, 0.12)

    def test_return_stroke_is_ignored(self):
        # swipe right then immediately bring the hand back
        frames = (still(0.3, 0.5, 0.3) + move(0.3, 0.7, 0.5, 0.5, 0.2)
                  + move(0.7, 0.3, 0.5, 0.5, 0.25) + still(0.3, 0.5, 0.3))
        self.assertEqual(run(GestureDetector(), frames), ["R"])

    def test_double_swipe_same_direction(self):
        frames = (still(0.2, 0.5, 0.3) + move(0.2, 0.45, 0.5, 0.5, 0.15) + still(0.45, 0.5, 0.25)
                  + move(0.45, 0.7, 0.5, 0.5, 0.15) + still(0.7, 0.5, 0.3))
        self.assertEqual(run(GestureDetector(), frames), ["R", "R"])

    def test_swipe_down_drops_and_up_rotates(self):
        d = GestureDetector()
        self.assertEqual(run(d, still(0.5, 0.3, 0.3) + move(0.5, 0.5, 0.3, 0.7, 0.2) + still(0.5, 0.7, 0.5)), ["H"])
        self.assertEqual(run(d, move(0.5, 0.5, 0.7, 0.3, 0.2) + still(0.5, 0.3, 0.5), t0=2), ["U"])

    def test_vertical_swipes_can_be_disabled(self):
        cfg = Config()
        cfg.vertical_swipes = False
        frames = still(0.5, 0.3, 0.3) + move(0.5, 0.5, 0.3, 0.7, 0.2)
        self.assertEqual(run(GestureDetector(cfg), frames), [])

    def test_distance_invariant(self):
        # A small hand (far from camera) making a proportionally small swipe
        small = lambda x, y: open_hand(x, y, size=0.06)  # noqa: E731
        frames = still(0.4, 0.5, 0.3, small) + move(0.4, 0.56, 0.5, 0.5, 0.2, small)
        self.assertEqual(run(GestureDetector(), frames), ["R"])

    def test_hand_lost_resets(self):
        frames = still(0.3, 0.5, 0.3) + [None] * 5 + still(0.7, 0.5, 0.3)
        self.assertEqual(run(GestureDetector(), frames), [])


class PoseTests(unittest.TestCase):
    def test_pinch_rotates_once(self):
        frames = still(0.5, 0.5, 0.3) + still(0.5, 0.5, 0.5, pinch) + still(0.5, 0.5, 0.3)
        self.assertEqual(run(GestureDetector(), frames), ["U"])

    def test_fist_hold_selects_once(self):
        self.assertTrue(GestureDetector.is_fist(fist(0.5, 0.5)))
        self.assertFalse(GestureDetector.is_fist(open_hand(0.5, 0.5)))
        frames = still(0.5, 0.5, 0.3) + still(0.5, 0.5, 1.5, fist)
        self.assertEqual(run(GestureDetector(), frames), ["C"])

    def test_short_fist_does_nothing(self):
        frames = still(0.5, 0.5, 0.3) + still(0.5, 0.5, 0.2, fist) + still(0.5, 0.5, 0.3)
        self.assertEqual(run(GestureDetector(), frames), [])


class PositionModeTests(unittest.TestCase):
    def test_columns_follow_hand(self):
        d = GestureDetector(mode="position")
        self.assertEqual(d.update(open_hand(0.2, 0.5), 0), ["@0"])
        d.reset()
        self.assertEqual(d.update(open_hand(0.5, 0.5), 0), ["@5"])
        d.reset()
        self.assertEqual(d.update(open_hand(0.79, 0.5), 0), ["@9"])

    def test_no_horizontal_swipes_in_position_mode(self):
        d = GestureDetector(mode="position")
        cmds = run(d, still(0.3, 0.5, 0.3) + move(0.3, 0.7, 0.5, 0.5, 0.2))
        self.assertEqual([c for c in cmds if not c.startswith("@")], [])

    def test_hysteresis(self):
        d = GestureDetector(mode="position")
        col = lambda x: int(d.update(open_hand(x, 0.5), 0)[0][1:])  # noqa: E731
        first = col(0.5)
        # palm x jittering around a column boundary shouldn't flicker
        self.assertEqual({col(0.5 + dx) for dx in (0.01, -0.01, 0.015, -0.015)}, {first})

    def test_zigzag_with_a_pause_works(self):
        d = GestureDetector()
        frames = (still(0.3, 0.5, 0.3) + move(0.3, 0.6, 0.5, 0.5, 0.15) + still(0.6, 0.5, 0.35)
                  + move(0.6, 0.3, 0.5, 0.5, 0.15) + still(0.3, 0.5, 0.3))
        self.assertEqual(run(d, frames), ["R", "L"])


if __name__ == "__main__":
    unittest.main()
