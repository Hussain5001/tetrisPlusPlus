"""Unit tests for the gesture detector using synthetic hand movements.

Run with:  python3 -m unittest gesture/test_gestures.py   (from the repo root)
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import math  # noqa: E402

from gestures import MODES, Config, GestureDetector  # noqa: E402

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
        self.assertEqual(run(GestureDetector(mode="palm"), still(0.5, 0.5, 2)), [])

    def test_slow_drift_sends_nothing(self):
        self.assertEqual(run(GestureDetector(mode="palm"), move(0.3, 0.7, 0.5, 0.5, 3)), [])

    def test_swipe_right_and_left(self):
        d = GestureDetector(mode="palm")
        cmds = run(d, still(0.3, 0.5, 0.3) + move(0.3, 0.7, 0.5, 0.5, 0.2) + still(0.7, 0.5, 0.5))
        self.assertEqual(cmds, ["R"])
        cmds = run(d, move(0.7, 0.3, 0.5, 0.5, 0.2) + still(0.3, 0.5, 0.5), t0=2)
        self.assertEqual(cmds, ["L"])

    def test_one_long_swipe_counts_once(self):
        cmds = run(GestureDetector(mode="palm"), still(0.1, 0.5, 0.3) + move(0.1, 0.9, 0.5, 0.5, 0.5))
        self.assertEqual(cmds, ["R"])

    def test_swipe_fires_early(self):
        # The command should be sent within the first ~100 ms of the movement
        d = GestureDetector(mode="palm")
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
        self.assertEqual(run(GestureDetector(mode="palm"), frames), ["R"])

    def test_double_swipe_same_direction(self):
        frames = (still(0.2, 0.5, 0.3) + move(0.2, 0.45, 0.5, 0.5, 0.15) + still(0.45, 0.5, 0.25)
                  + move(0.45, 0.7, 0.5, 0.5, 0.15) + still(0.7, 0.5, 0.3))
        self.assertEqual(run(GestureDetector(mode="palm"), frames), ["R", "R"])

    def test_swipe_down_drops_and_up_rotates(self):
        d = GestureDetector(mode="palm")
        self.assertEqual(run(d, still(0.5, 0.3, 0.3) + move(0.5, 0.5, 0.3, 0.7, 0.2) + still(0.5, 0.7, 0.5)), ["H"])
        self.assertEqual(run(d, move(0.5, 0.5, 0.7, 0.3, 0.2) + still(0.5, 0.3, 0.5), t0=2), ["U"])

    def test_vertical_swipes_can_be_disabled(self):
        cfg = Config()
        cfg.vertical_swipes = False
        frames = still(0.5, 0.3, 0.3) + move(0.5, 0.5, 0.3, 0.7, 0.2)
        self.assertEqual(run(GestureDetector(cfg, mode="palm"), frames), [])

    def test_distance_invariant(self):
        # A small hand (far from camera) making a proportionally small swipe
        small = lambda x, y: open_hand(x, y, size=0.06)  # noqa: E731
        frames = still(0.4, 0.5, 0.3, small) + move(0.4, 0.56, 0.5, 0.5, 0.2, small)
        self.assertEqual(run(GestureDetector(mode="palm"), frames), ["R"])

    def test_hand_lost_resets(self):
        frames = still(0.3, 0.5, 0.3) + [None] * 5 + still(0.7, 0.5, 0.3)
        self.assertEqual(run(GestureDetector(mode="palm"), frames), [])


class PoseTests(unittest.TestCase):
    def test_pinch_rotates_once(self):
        for mode in MODES:
            frames = still(0.5, 0.5, 0.3) + still(0.5, 0.5, 0.5, pinch) + still(0.5, 0.5, 0.3)
            cmds = [c for c in run(GestureDetector(mode=mode), frames) if not c.startswith("@")]
            self.assertEqual(cmds, ["U"], mode)

    def test_fist_hold_selects_once(self):
        self.assertTrue(GestureDetector.is_fist(fist(0.5, 0.5)))
        self.assertFalse(GestureDetector.is_fist(open_hand(0.5, 0.5)))
        frames = still(0.5, 0.5, 0.3) + still(0.5, 0.5, 1.5, fist)
        self.assertEqual(run(GestureDetector(mode="palm"), frames), ["C"])

    def test_short_fist_does_nothing(self):
        frames = still(0.5, 0.5, 0.3) + still(0.5, 0.5, 0.2, fist) + still(0.5, 0.5, 0.3)
        self.assertEqual(run(GestureDetector(mode="palm"), frames), [])


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
        d = GestureDetector(mode="palm")
        frames = (still(0.3, 0.5, 0.3) + move(0.3, 0.6, 0.5, 0.5, 0.15) + still(0.6, 0.5, 0.35)
                  + move(0.6, 0.3, 0.5, 0.5, 0.15) + still(0.3, 0.5, 0.3))
        self.assertEqual(run(d, frames), ["R", "L"])



# ------------------------------------------------------------ index finger

def pointing(cx, cy, tilt=0.0, size=0.15, curl=0.0, pinched=False, length=0.8):
    """A hand pointing with the index finger. tilt: radians, 0 = straight up,
    positive = to the right (as seen on screen). curl: 0 = finger out,
    1 = curled into a fist. length: how long the finger looks (shorter when
    it points towards the camera)."""
    pts = fist(cx, cy, size)
    mcp = pts[5]
    d = (math.sin(tilt), -math.cos(tilt))
    k = length / 0.8
    out = [(mcp[0] + f * k * size * d[0], mcp[1] + f * k * size * d[1])
           for f in (0.35, 0.6, 0.8)]
    for i, (x, y) in enumerate(out):
        cx_, cy_ = fist(cx, cy, size)[6 + i]
        pts[6 + i] = (x + (cx_ - x) * curl, y + (cy_ - y) * curl)
    if pinched:
        pts[4] = (pts[8][0] + 0.01, pts[8][1])
    return pts


def tilt(a0, a1, seconds, cx=0.5, cy=0.5):
    n = max(int(seconds * FPS), 1)
    return [pointing(cx, cy, a0 + (a1 - a0) * i / n) for i in range(1, n + 1)]


def hold(angle, seconds, cx=0.5, cy=0.5, **kw):
    return [pointing(cx, cy, angle, **kw) for _ in range(int(seconds * FPS))]


def run_timed(detector, frames, t0=0.0):
    out = []
    for i, lm in enumerate(frames):
        for c in detector.update(lm, t0 + i / FPS):
            out.append((t0 + i / FPS, c))
    return out


FLICK = 0.6  # radians (~34 deg), past point mode's ~22 deg threshold


def swipe(x0, x1, seconds=0.15, y=0.5):
    """The pointing hand moving sideways (the fingertip moves with it)."""
    return move(x0, x1, y, y, seconds, pointing)[1:]


def rest(x, seconds, y=0.5):
    return still(x, y, seconds, pointing)


def curl_to(c0, c1, seconds, cx=0.5, cy=0.5):
    n = max(int(seconds * FPS), 1)
    return [pointing(cx, cy, curl=c0 + (c1 - c0) * i / n) for i in range(1, n + 1)]


class PointingPoseTests(unittest.TestCase):
    def test_pointing_is_recognised(self):
        self.assertTrue(GestureDetector.is_pointing(pointing(0.5, 0.5)))
        self.assertTrue(GestureDetector.is_pointing(pointing(0.5, 0.5, FLICK)))
        self.assertTrue(GestureDetector.is_pointing(pointing(0.5, 0.5, -FLICK)))
        self.assertFalse(GestureDetector.is_pointing(open_hand(0.5, 0.5)))
        self.assertFalse(GestureDetector.is_pointing(fist(0.5, 0.5)))
        self.assertFalse(GestureDetector.is_pointing(pointing(0.5, 0.5, curl=1.0)))

    def test_open_palm_is_recognised(self):
        self.assertTrue(GestureDetector.is_open_palm(open_hand(0.5, 0.5)))
        self.assertFalse(GestureDetector.is_open_palm(fist(0.5, 0.5)))
        self.assertFalse(GestureDetector.is_open_palm(pointing(0.5, 0.5)))

    def test_open_palm_swipes_do_nothing_in_finger_modes(self):
        for mode in ("flick", "point"):
            frames = still(0.3, 0.5, 0.3) + move(0.3, 0.7, 0.5, 0.5, 0.2) + still(0.7, 0.5, 0.3) \
                + move(0.7, 0.7, 0.5, 0.8, 0.15) + still(0.7, 0.8, 0.3)
            self.assertEqual(run(GestureDetector(mode=mode), frames), [], mode)

    def test_modes(self):
        self.assertEqual(GestureDetector().mode, "flick")
        self.assertEqual(GestureDetector(mode="swipe").mode, "palm")
        with self.assertRaises(ValueError):
            GestureDetector(mode="wave")
        d = GestureDetector()
        seen = []
        for _ in MODES:
            seen.append(d.mode)
            d.toggle_mode()
        self.assertEqual(seen, ["flick", "point", "palm", "position"])
        self.assertEqual(d.mode, "flick")


class FlickTests(unittest.TestCase):
    """Flick mode: swipe the pointing finger left / right."""

    def test_swipe_right_and_left(self):
        self.assertEqual(run(GestureDetector(), rest(0.5, 0.3) + swipe(0.5, 0.6) + rest(0.6, 0.4)), ["R"])
        self.assertEqual(run(GestureDetector(), rest(0.5, 0.3) + swipe(0.5, 0.4) + rest(0.4, 0.4)), ["L"])

    def test_tilting_the_finger_is_a_swipe_too(self):
        self.assertEqual(run(GestureDetector(), hold(0, 0.3) + tilt(0, FLICK, 0.1) + hold(FLICK, 0.4)), ["R"])
        self.assertEqual(run(GestureDetector(), hold(0, 0.3) + tilt(0, -FLICK, 0.1) + hold(-FLICK, 0.4)), ["L"])

    def test_swipe_fires_early(self):
        d = GestureDetector()
        frames = rest(0.5, 0.3) + swipe(0.5, 0.7, 0.25)
        fired_at = next(i for i, lm in enumerate(frames) if d.update(lm, i / FPS)) / FPS - 0.3
        self.assertLess(fired_at, 0.15)

    def test_bringing_the_finger_back_is_not_a_swipe(self):
        # fast and slow returns, straight away or after waiting at the side
        for wait in (0.0, 1.0):
            for back in (0.1, 0.5):
                frames = rest(0.5, 0.3) + swipe(0.5, 0.6) + rest(0.6, wait) + swipe(0.6, 0.5, back) \
                    + rest(0.5, 0.4)
                self.assertEqual(run(GestureDetector(), frames), ["R"], (wait, back))
        frames = hold(0, 0.3) + tilt(0, -FLICK, 0.1) + tilt(-FLICK, 0, 0.1) + hold(0, 0.4)
        self.assertEqual(run(GestureDetector(), frames), ["L"])

    def test_repeated_swipes(self):
        frames = rest(0.5, 0.3)
        for _ in range(3):
            frames += swipe(0.5, 0.6) + swipe(0.6, 0.5)
        self.assertEqual(run(GestureDetector(), frames + rest(0.5, 0.3)), ["R", "R", "R"])
        frames = hold(0, 0.3)
        for _ in range(3):
            frames += tilt(0, -FLICK, 0.1) + tilt(-FLICK, 0, 0.1)
        self.assertEqual(run(GestureDetector(), frames), ["L", "L", "L"])

    def test_left_then_right(self):
        # back to the middle, then on to the right, without stopping
        frames = rest(0.5, 0.3) + swipe(0.5, 0.4) + swipe(0.4, 0.5) + swipe(0.5, 0.6) + rest(0.6, 0.3)
        self.assertEqual(run(GestureDetector(), frames), ["L", "R"])
        # one swing from the left straight across to the right
        frames = rest(0.5, 0.3) + swipe(0.5, 0.4) + swipe(0.4, 0.6, 0.25) + rest(0.6, 0.3)
        self.assertEqual(run(GestureDetector(), frames), ["L", "R"])

    def test_two_swipes_then_one_return(self):
        frames = rest(0.5, 0.3) + swipe(0.5, 0.42) + rest(0.42, 0.2) + swipe(0.42, 0.34) \
            + rest(0.34, 0.3) + swipe(0.34, 0.5, 0.3) + rest(0.5, 0.3)
        self.assertEqual(run(GestureDetector(), frames), ["L", "L"])

    def test_one_long_swipe_counts_once(self):
        frames = rest(0.3, 0.3) + swipe(0.3, 0.7, 0.5) + rest(0.7, 0.3)
        self.assertEqual(run(GestureDetector(), frames), ["R"])

    def test_slow_drift_and_jitter_do_nothing(self):
        self.assertEqual(run(GestureDetector(), rest(0.4, 0.3) + swipe(0.4, 0.6, 2.0)), [])
        frames = rest(0.5, 0.3)
        for i in range(40):
            frames += rest(0.5 + (0.01 if i % 2 else -0.01), 1 / FPS)
        self.assertEqual(run(GestureDetector(), frames), [])

    def test_diagonal_is_not_a_side_swipe(self):
        frames = rest(0.5, 0.3) + move(0.5, 0.56, 0.5, 0.35, 0.15, pointing)[1:] + rest(0.56, 0.3, 0.35)
        self.assertEqual(run(GestureDetector(), frames), [])

    def test_small_or_far_away_hand(self):
        small = lambda x, y: pointing(x, y, size=0.06)  # noqa: E731
        frames = still(0.5, 0.5, 0.3, small) + move(0.5, 0.54, 0.5, 0.5, 0.15, small) \
            + still(0.54, 0.5, 0.3, small)
        self.assertEqual(run(GestureDetector(), frames), ["R"])

    def test_curling_the_finger_lets_you_move_your_hand_back(self):
        # swipe right, curl the finger (rest), put the hand back in the
        # middle, point again: the next swipe right works straight away
        frames = rest(0.5, 0.3) + swipe(0.5, 0.6) + rest(0.6, 0.2) \
            + still(0.6, 0.5, 0.6, fist) + still(0.5, 0.5, 0.3, fist) + rest(0.5, 0.3) \
            + swipe(0.5, 0.6) + rest(0.6, 0.3)
        self.assertEqual(run(GestureDetector(), frames), ["R", "R"])

    def test_short_tracking_gap_keeps_the_swipe(self):
        frames = rest(0.5, 0.3) + swipe(0.5, 0.53, 0.05) + [None, None] + swipe(0.57, 0.6, 0.05) \
            + rest(0.6, 0.3)
        self.assertEqual(run(GestureDetector(), frames), ["R"])

    def test_long_tracking_gap_starts_over(self):
        frames = rest(0.5, 0.4) + [None] * 8 + rest(0.6, 0.4)
        self.assertEqual(run(GestureDetector(), frames), [])


class NoisyTrackingTests(unittest.TestCase):
    """Real landmarks wobble by a few pixels every frame."""

    @staticmethod
    def noisy(frames, amount=0.004, seed=1):
        import random
        rng = random.Random(seed)
        return [[(x + rng.uniform(-amount, amount), y + rng.uniform(-amount, amount))
                 for x, y in lm] for lm in frames]

    def test_resting_finger_sends_nothing(self):
        for mode in ("flick", "point"):
            self.assertEqual(run(GestureDetector(mode=mode), self.noisy(rest(0.5, 10))), [], mode)

    def test_swipes_and_drops_still_work(self):
        frames = rest(0.5, 0.4) + swipe(0.5, 0.6) + rest(0.6, 0.3) + swipe(0.6, 0.5, 0.4) \
            + rest(0.5, 0.4) + swipe(0.5, 0.4) + rest(0.4, 0.3) + swipe(0.4, 0.5, 0.4) \
            + rest(0.5, 0.4) + curl_to(0, 0.6, 0.1) + curl_to(0.6, 0, 0.1) + hold(0, 0.4)
        for seed in range(5):
            self.assertEqual(run(GestureDetector(), self.noisy(frames, seed=seed)),
                             ["R", "L", "H"], seed)


class PointModeTests(unittest.TestCase):
    def test_holding_repeats(self):
        d = GestureDetector(mode="point")
        frames = hold(0, 0.3) + tilt(0, -FLICK, 0.05) + hold(-FLICK, 1.0) + tilt(-FLICK, 0, 0.05) \
            + hold(0, 0.5)
        events = run_timed(d, frames)
        self.assertTrue(all(c == "L" for _, c in events))
        times = [t for t, _ in events]
        self.assertGreaterEqual(len(times), 5)
        self.assertLess(times[0], 0.4)                       # right away
        self.assertAlmostEqual(times[1] - times[0], 0.35, delta=0.04)
        self.assertAlmostEqual(times[2] - times[1], 0.12, delta=0.04)
        self.assertLess(times[-1], 0.3 + 0.05 + 1.0 + 0.05)  # stops on release

    def test_a_quick_tap_moves_once(self):
        frames = hold(0, 0.3) + tilt(0, FLICK, 0.05) + hold(FLICK, 0.15) + tilt(FLICK, 0, 0.05) \
            + hold(0, 0.4)
        self.assertEqual(run(GestureDetector(mode="point"), frames), ["R"])

    def test_slow_tilt_still_moves(self):
        frames = hold(0, 0.3) + tilt(0, FLICK, 1.0) + hold(FLICK, 0.1)
        moves = run(GestureDetector(mode="point"), frames)
        self.assertGreaterEqual(len(moves), 1)
        self.assertEqual(set(moves), {"R"})

    def test_swing_straight_across(self):
        frames = hold(0, 0.3) + tilt(0, -FLICK, 0.05) + hold(-FLICK, 0.1) \
            + tilt(-FLICK, FLICK, 0.1) + hold(FLICK, 0.1)
        self.assertEqual(run(GestureDetector(mode="point"), frames), ["L", "R"])

    def test_a_tilted_resting_finger_is_learned(self):
        frames = hold(0.45, 0.6) + tilt(0.45, -0.15, 0.1) + hold(-0.15, 0.3)
        self.assertEqual(run(GestureDetector(mode="point"), frames), ["L"])

    def test_moving_the_whole_hand_does_not_move(self):
        frames = rest(0.5, 0.3) + swipe(0.5, 0.6) + rest(0.6, 0.3) + swipe(0.6, 0.5) + rest(0.5, 0.3)
        self.assertEqual(run(GestureDetector(mode="point"), frames), [])


class FingerDropRotatePauseTests(unittest.TestCase):
    def test_tapping_the_finger_down_drops(self):
        for mode in ("flick", "point"):
            frames = hold(0, 0.3) + curl_to(0, 0.6, 0.1) + curl_to(0.6, 0, 0.1) + hold(0, 0.4)
            self.assertEqual(run(GestureDetector(mode=mode), frames), ["H"], mode)

    def test_the_tap_fires_when_the_finger_is_back(self):
        d = GestureDetector()
        frames = hold(0, 0.3) + curl_to(0, 0.6, 0.1) + curl_to(0.6, 0, 0.1) + hold(0, 0.4)
        events = run_timed(d, frames)
        self.assertEqual(len(events), 1)
        self.assertLess(events[0][0], 0.3 + 0.2 + 0.15)

    def test_swiping_the_hand_down_drops(self):
        for mode in ("flick", "point"):
            frames = rest(0.5, 0.3, 0.4) + move(0.5, 0.5, 0.4, 0.5, 0.1, pointing)[1:] \
                + rest(0.5, 0.4, 0.5)
            self.assertEqual(run(GestureDetector(mode=mode), frames), ["H"], mode)

    def test_one_drop_per_movement(self):
        frames = rest(0.5, 0.3, 0.3) + move(0.5, 0.5, 0.3, 0.7, 0.3, pointing)[1:] + rest(0.5, 0.4, 0.7)
        self.assertEqual(run(GestureDetector(), frames), ["H"])

    def test_two_taps_drop_twice(self):
        frames = hold(0, 0.3)
        for _ in range(2):
            frames += curl_to(0, 0.6, 0.1) + curl_to(0.6, 0, 0.1) + hold(0, 0.3)
        self.assertEqual(run(GestureDetector(), frames), ["H", "H"])

    def test_resting_the_hand_does_nothing(self):
        # curl the finger into a fist and leave it there, then point again
        frames = hold(0, 0.3) + curl_to(0, 1, 0.15) + still(0.5, 0.5, 1.5, fist) \
            + curl_to(1, 0, 0.2) + hold(0, 0.4)
        self.assertEqual(run(GestureDetector(), frames), [])
        # a fist doesn't pause/select in the finger modes
        self.assertEqual(run(GestureDetector(mode="point"), still(0.5, 0.5, 2, fist)), [])

    def test_slow_moves_and_moving_up_do_nothing(self):
        slow = rest(0.5, 0.3, 0.4) + move(0.5, 0.5, 0.4, 0.6, 1.5, pointing)
        self.assertEqual(run(GestureDetector(), slow), [])
        up = rest(0.5, 0.3, 0.6) + move(0.5, 0.5, 0.6, 0.4, 0.1, pointing) + rest(0.5, 0.3, 0.4)
        self.assertEqual(run(GestureDetector(), up), [])
        slow_curl = hold(0, 0.3) + curl_to(0, 0.6, 1.5) + curl_to(0.6, 0, 0.2) + hold(0, 0.3)
        self.assertEqual(run(GestureDetector(), slow_curl), [])

    def test_poking_towards_the_camera_does_not_drop(self):
        poke = [pointing(0.5, 0.5, length=L) for L in (0.6, 0.46, 0.46, 0.65, 0.8)]
        frames = hold(0, 0.4) + poke + hold(0, 0.4)
        self.assertEqual(run(GestureDetector(), frames), [])

    def test_pinch_rotates_without_moving_or_dropping(self):
        # the index finger bends down and sideways to meet the thumb
        pinch_down = [pointing(0.5, 0.5, 0.3 * c, curl=0.6 * c, pinched=c > 0.5)
                      for c in (0.25, 0.5, 0.75, 1, 1, 1, 1, 1, 1)]
        release = list(reversed(pinch_down))
        for mode in ("flick", "point"):
            frames = hold(0, 0.3) + pinch_down + release + hold(0, 0.4)
            self.assertEqual(run(GestureDetector(mode=mode), frames), ["U"], mode)

    def test_finger_swinging_back_after_a_pinch_is_not_a_swipe(self):
        # the finger leans over to the thumb, then swings back on release
        lean = [pointing(0.5, 0.5, -FLICK * c, pinched=c > 0.5) for c in (0.3, 0.6, 1, 1, 1, 1, 1)]
        frames = hold(0, 0.3) + lean + [pointing(0.5, 0.5, -FLICK)] + tilt(-FLICK, 0, 0.1) + hold(0, 0.4)
        self.assertEqual(run(GestureDetector(), frames), ["U"])

    def test_open_palm_pauses(self):
        for mode in ("flick", "point"):
            frames = rest(0.5, 0.3) + still(0.5, 0.5, 2.0) + rest(0.5, 0.3)
            self.assertEqual(run(GestureDetector(mode=mode), frames), ["P"], mode)
        # it has to be held for about a second
        self.assertEqual(run(GestureDetector(), rest(0.5, 0.3) + still(0.5, 0.5, 0.7)), [])

    def test_palm_pauses_only_when_held_still(self):
        frames = still(0.2, 0.5, 0.2) + move(0.2, 0.8, 0.5, 0.5, 1.5)
        self.assertEqual(run(GestureDetector(), frames), [])

    def test_drop_can_be_disabled(self):
        cfg = Config()
        cfg.vertical_swipes = False
        frames = hold(0, 0.3) + curl_to(0, 0.6, 0.1) + curl_to(0.6, 0, 0.1) + hold(0, 0.4)
        self.assertEqual(run(GestureDetector(cfg), frames), [])


if __name__ == "__main__":
    unittest.main()
