"""Tests for follow mode, the default way to play by hand: the open hand's
position is the pointer, a pinch rotates, a fist drops.

Run with:  py -m unittest gesture/test_follow.py   (from the repo root)
"""
import math
import os
import random
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gestures import (Config, GestureDetector, OneEuro, finger_curl,  # noqa: E402
                      pinch_gap)
from test_gestures import FPS  # noqa: E402

SIZE = 0.15                         # hand size of the test poses (picture heights)
STEP = SIZE / Config().aspect       # one hand size sideways, in picture widths
KNUCKLES = (-0.25, -0.05, 0.15, 0.32)   # index .. pinky, in hand sizes
BONES = (0.35, 0.25, 0.20)              # the three bones of a finger
BENDS = (1.2, 1.75, 1.2)                # how far each joint bends in a fist (radians)


def shape(grip=0.0, pinched=False):
    """21 landmarks of a hand in hand sizes, with the middle of the palm at
    (0, 0). grip: how far the fingers are curled in, 0 = open, 1 = fist; one
    number, or four for index .. pinky. Each finger bends at its three
    joints like a real one. pinched: the thumb tip is on the index fingertip."""
    grips = grip if isinstance(grip, (tuple, list)) else (grip,) * 4
    pts = [(0.0, 0.5), (-0.35, 0.3), (-0.55, 0.1), (-0.7, -0.05), (-0.85, -0.2)]
    for knuckle, g in zip(KNUCKLES, grips):
        x, y, angle = knuckle, -0.5, 0.0
        pts.append((x, y))
        for bone, bend in zip(BONES, BENDS):
            angle += g * bend
            x, y = x + bone * math.sin(angle), y - bone * math.cos(angle)
            pts.append((x, y))
    if pinched:
        pts[4] = (pts[8][0] + 0.02, pts[8][1])
    palm_x = sum(pts[i][0] for i in (0, 5, 9, 13, 17)) / 5
    palm_y = sum(pts[i][1] for i in (0, 5, 9, 13, 17)) / 5
    return [(x - palm_x, y - palm_y) for x, y in pts]


def hand(cx, cy, grip=0.0, pinched=False, size=SIZE):
    """That hand in the camera picture (0..1 both ways), `size` picture
    heights big, with the middle of its palm at (cx, cy)."""
    return [(cx + x * size / Config().aspect, cy + y * size) for x, y in shape(grip, pinched)]


def hold(seconds, cx=0.5, cy=0.5, **pose):
    return [hand(cx, cy, **pose) for _ in range(max(int(seconds * FPS), 1))]


def close_hand(seconds, cx=0.5, cy=0.5, start=0.0, end=1.0, drift=0.0, **pose):
    """The fingers curl from `start` to `end`; the hand moves sideways by
    `drift` picture widths while they do."""
    n = max(int(seconds * FPS), 1)
    return [hand(cx + drift * i / n, cy, grip=start + (end - start) * i / n, **pose)
            for i in range(1, n + 1)]


def play(detector, frames, t0=0.0):
    """Feeds the frames. Returns (commands, pointers): the one-letter
    commands in order, and the (u, v) pointer of every frame."""
    commands, pointers = [], []
    for i, lm in enumerate(frames):
        for c in detector.update(lm, t0 + i / FPS):
            if len(c) == 1:
                commands.append(c)
            else:
                x, y = c[1:].split("Y")
                pointers.append((int(x) / 999, int(y) / 999))
    return commands, pointers


class HandFeatureTests(unittest.TestCase):
    def test_curl_of_open_and_closed_fingers(self):
        for finger in ("index", "middle", "ring", "pinky"):
            self.assertLess(finger_curl(shape(), finger), 0.05, finger)
            self.assertGreater(finger_curl(shape(grip=1.0), finger), 0.8, finger)
        # more bend is always more curl; a relaxed hand is far from a fist
        values = [finger_curl(shape(grip=g), "middle") for g in (0, 0.25, 0.5, 0.75, 1)]
        self.assertEqual(values, sorted(values))
        self.assertLess(values[2], 0.35)
        # one finger at a time
        one = shape(grip=(0, 1, 0, 1))
        self.assertLess(finger_curl(one, "index"), 0.05)
        self.assertGreater(finger_curl(one, "middle"), 0.8)

    def test_curl_does_not_depend_on_how_the_hand_is_turned_or_how_big_it_is(self):
        def turned(points, angle, scale):
            c, s = math.cos(angle), math.sin(angle)
            return [(scale * (c * x - s * y), scale * (s * x + c * y)) for x, y in points]

        for pose in (shape(), shape(grip=1.0), shape(grip=0.5)):
            straight = finger_curl(pose, "index")
            for angle in (0.7, 1.6, 3.0):
                self.assertAlmostEqual(finger_curl(turned(pose, angle, 2.5), "index"), straight)

    def test_curl_uses_depth_when_it_is_there(self):
        # a finger pointing at the camera looks short in the picture, but its
        # joints are still in a straight line
        flat = [(0.0, 0.0, 0.0)] * 21
        towards = list(flat)
        for joint, depth in zip((5, 6, 7, 8), (0.0, -0.03, -0.05, -0.07)):
            towards[joint] = (0.0, 0.0, depth)
        self.assertAlmostEqual(finger_curl(towards, "index"), 0.0)

    def test_pinch_gap_is_measured_in_hand_sizes(self):
        self.assertGreater(pinch_gap(shape()), 0.8)
        self.assertLess(pinch_gap(shape(pinched=True)), 0.1)
        far_away = [(x * 0.05, y * 0.05) for x, y in shape(pinched=True)]
        self.assertAlmostEqual(pinch_gap(far_away), pinch_gap(shape(pinched=True)))


class OneEuroTests(unittest.TestCase):
    def test_steady_at_rest(self):
        rng = random.Random(3)
        f = OneEuro(1.2, 4.0)
        out = [f(0.5 + rng.uniform(-0.01, 0.01), i / FPS) for i in range(150)][30:]
        self.assertLess(max(out) - min(out), 0.012)      # the input wobbles by 0.02

    def test_quick_when_it_moves(self):
        f = OneEuro(1.2, 4.0)
        for i in range(30):
            f(0.2, i / FPS)
        # the hand jumps across the board in 0.1 s
        values = [f(0.2 + 0.6 * min(1, (i + 1) / 3), 1 + i / FPS) for i in range(8)]
        self.assertGreater(values[4], 0.72)              # within 10% in 5 frames
        self.assertLessEqual(max(values), 0.8001)        # and it never overshoots


class PointerTests(unittest.TestCase):
    def test_the_hand_in_the_middle_points_at_the_middle(self):
        _, pointers = play(GestureDetector(), hold(0.5))
        u, v = pointers[-1]
        self.assertAlmostEqual(u, 0.5, delta=0.002)
        self.assertAlmostEqual(v, 0.5, delta=0.002)
        self.assertEqual(len(pointers), 15)              # one pointer on every frame

    def test_the_pointer_follows_the_hand(self):
        d = GestureDetector()
        _, middle = play(d, hold(0.5))
        _, right = play(d, hold(0.5, cx=0.5 + STEP), t0=0.5)       # one hand size right
        _, left = play(d, hold(0.5, cx=0.5 - STEP), t0=1.0)
        third = 1 / Config().zone_width                            # of the board
        self.assertAlmostEqual(right[-1][0] - middle[-1][0], third, delta=0.02)
        self.assertAlmostEqual(left[-1][0] - middle[-1][0], -third, delta=0.02)
        _, up = play(d, hold(0.5, cy=0.5 - SIZE), t0=1.5)
        self.assertAlmostEqual(up[-1][1] - middle[-1][1], -1 / Config().zone_height, delta=0.03)

    def test_it_stops_at_the_edges(self):
        d = GestureDetector()
        _, far_right = play(d, hold(0.5, cx=0.95))
        _, far_left = play(d, hold(0.5, cx=0.05), t0=0.5)
        self.assertEqual(far_right[-1][0], 1.0)
        self.assertEqual(far_left[-1][0], 0.0)

    def test_the_same_from_close_up_or_far_away(self):
        for size in (0.06, 0.25):
            d = GestureDetector()
            step = size / Config().aspect
            _, a = play(d, hold(0.5, size=size))
            _, b = play(d, hold(0.5, cx=0.5 + step, size=size), t0=0.5)
            self.assertAlmostEqual(b[-1][0] - a[-1][0], 1 / 3, delta=0.02, msg=size)

    def test_reach_changes_how_far_the_hand_has_to_move(self):
        cfg = Config()
        cfg.zone_width = 1.5
        d = GestureDetector(cfg)
        _, a = play(d, hold(0.5))
        _, b = play(d, hold(0.5, cx=0.5 + STEP / 2), t0=0.5)
        self.assertAlmostEqual(b[-1][0] - a[-1][0], 1 / 3, delta=0.02)

    def test_a_trembling_hand_stays_in_its_column(self):
        rng = random.Random(1)
        frames = [[(x + rng.uniform(-0.004, 0.004), y + rng.uniform(-0.004, 0.004))
                   for x, y in hand(0.5, 0.5)] for _ in range(300)]
        commands, pointers = play(GestureDetector(), frames)
        us = [u for u, _ in pointers[30:]]
        self.assertEqual(commands, [])
        self.assertLess(max(us) - min(us), 0.03)         # a column is 0.1 wide

    def test_recentre_puts_the_middle_under_the_hand(self):
        d = GestureDetector()
        _, before = play(d, hold(0.5, cx=0.3, cy=0.4))
        d.recentre()
        _, after = play(d, hold(0.5, cx=0.3, cy=0.4), t0=1.0)
        self.assertLess(before[-1][0], 0.1)
        self.assertAlmostEqual(after[-1][0], 0.5, delta=0.01)
        self.assertAlmostEqual(after[-1][1], 0.5, delta=0.01)

    def test_hand_gone(self):
        d = GestureDetector()
        self.assertEqual(d.update(None, 0), [])
        play(d, hold(0.3, cx=0.3))
        # a short gap (blur) changes nothing; after a long one it starts fresh
        for i in range(3):
            self.assertEqual(d.update(None, 0.3 + i / FPS), [])
        self.assertIsNotNone(d.follow.size)
        for i in range(6):
            d.update(None, 0.5 + i / FPS)
        self.assertIsNone(d.follow.size)


class PinchTests(unittest.TestCase):
    def test_a_pinch_rotates_once(self):
        frames = hold(0.3) + hold(0.5, pinched=True) + hold(0.3)
        self.assertEqual(play(GestureDetector(), frames)[0], ["U"])

    def test_it_rotates_on_contact(self):
        d = GestureDetector()
        play(d, hold(0.3))
        self.assertIn("U", d.update(hand(0.5, 0.5, pinched=True), 0.3))

    def test_two_pinches_rotate_twice(self):
        frames = hold(0.3)
        for _ in range(2):
            frames += hold(0.1, pinched=True) + hold(0.1)
        self.assertEqual(play(GestureDetector(), frames)[0], ["U", "U"])

    def test_the_pointer_stays_put_while_pinching(self):
        d = GestureDetector()
        _, before = play(d, hold(0.4))
        _, during = play(d, hold(0.3, pinched=True), t0=0.4)
        self.assertAlmostEqual(during[-1][0], before[-1][0], delta=0.005)

    def test_a_curled_hand_does_not_pinch(self):
        # the thumb rests on the index finger of a loosely closed hand
        frames = hold(0.3, grip=0.75) + hold(0.5, grip=0.75, pinched=True) + hold(0.3, grip=0.75)
        self.assertNotIn("U", play(GestureDetector(), frames)[0])


class FistTests(unittest.TestCase):
    def test_closing_the_hand_drops_once(self):
        frames = hold(0.3) + close_hand(0.12) + hold(1.0, grip=1.0)
        self.assertEqual(play(GestureDetector(), frames)[0], ["H"])

    def test_open_and_close_again_drops_again(self):
        frames = hold(0.3)
        for _ in range(3):
            frames += close_hand(0.1) + hold(0.1, grip=1.0) + close_hand(0.1, start=1.0, end=0.0)
        self.assertEqual(play(GestureDetector(), frames)[0], ["H", "H", "H"])

    def test_the_drop_needs_two_frames(self):
        d = GestureDetector()
        play(d, hold(0.3))
        first = d.update(hand(0.5, 0.5, grip=1.0), 0.3)
        second = d.update(hand(0.5, 0.5, grip=1.0), 0.3 + 1 / FPS)
        self.assertNotIn("H", first)
        self.assertIn("H", second)
        # a single bad frame in between an open hand is not a drop
        frames = hold(0.3) + [hand(0.5, 0.5, grip=1.0)] + hold(0.3)
        self.assertEqual(play(GestureDetector(), frames)[0], [])

    def test_a_relaxed_hand_does_not_drop(self):
        # fingers resting half curled, opening and curling a little
        frames = hold(0.3)
        for _ in range(4):
            frames += close_hand(0.2, end=0.45) + hold(0.3, grip=0.45) \
                + close_hand(0.2, start=0.45, end=0.1)
        self.assertEqual(play(GestureDetector(), frames)[0], [])

    def test_a_pointing_finger_is_not_a_fist(self):
        frames = hold(1.0, grip=(0, 1, 1, 1))
        self.assertEqual(play(GestureDetector(), frames)[0], [])

    def test_closing_a_fist_never_rotates(self):
        # the thumb folds over the index finger as the hand closes
        closing = [hand(0.5, 0.5, grip=g, pinched=g >= 0.8) for g in (0.2, 0.4, 0.6, 0.8, 1.0)]
        frames = hold(0.3) + closing + hold(0.3, grip=1.0, pinched=True) \
            + list(reversed(closing)) + hold(0.3)
        self.assertEqual(play(GestureDetector(), frames)[0], ["H"])

    def test_pinch_then_fist_is_rotate_then_drop(self):
        frames = hold(0.3) + hold(0.1, pinched=True) + hold(0.1) + close_hand(0.1) \
            + hold(0.2, grip=1.0)
        self.assertEqual(play(GestureDetector(), frames)[0], ["U", "H"])

    def test_the_drop_lands_where_the_hand_was_before_it_closed(self):
        # closing tugs the hand a whole column sideways
        drift = 0.1 * Config().zone_width * STEP
        for seconds in (0.1, 0.2, 0.4):                 # a quick fist and a slow one
            d = GestureDetector()
            _, before = play(d, hold(0.5))
            frames = close_hand(seconds, drift=drift) + hold(0.3, cx=0.5 + drift, grip=1.0)
            dropped_at = None
            for i, lm in enumerate(frames):
                out = d.update(lm, 0.5 + i / FPS)
                if "H" in out:
                    dropped_at = int(out[0][1:].split("Y")[0]) / 999
                    break
            self.assertIsNotNone(dropped_at, seconds)
            self.assertAlmostEqual(dropped_at, before[-1][0], delta=0.03, msg=seconds)
            # afterwards the pointer is live again, at the hand
            _, after = play(d, hold(0.5, cx=0.5 + drift, grip=1.0), t0=2.0)
            self.assertAlmostEqual(after[-1][0], before[-1][0] + 0.1, delta=0.02, msg=seconds)

    def test_the_pointer_keeps_following_a_relaxed_hand(self):
        # fingers a little curled must not hold the piece back
        d = GestureDetector()
        _, a = play(d, hold(0.5, grip=0.5))
        _, b = play(d, hold(0.5, cx=0.5 + STEP, grip=0.5), t0=0.5)
        self.assertAlmostEqual(b[-1][0] - a[-1][0], 1 / 3, delta=0.02)

    def test_the_pointer_comes_before_the_drop(self):
        d = GestureDetector()
        play(d, hold(0.3) + [hand(0.5, 0.5, grip=1.0)])
        out = d.update(hand(0.5, 0.5, grip=1.0), 0.4)
        self.assertEqual((out[0][0], out[-1]), ("X", "H"))


class WorldLandmarkTests(unittest.TestCase):
    """With MediaPipe's 3D landmarks the finger poses are read from them."""

    @staticmethod
    def world(grip):
        """MediaPipe's world landmarks: metres, around the hand's centre."""
        return [(x * 0.09, y * 0.09, 0.0) for x, y in shape(grip)]

    def test_poses_come_from_the_world_landmarks(self):
        d = GestureDetector()
        t = 0.0
        out = []
        for _ in range(10):         # the picture shows an open hand, the 3D hand is a fist
            out += d.update(hand(0.5, 0.5), t, world=self.world(1.0))
            t += 1 / FPS
        self.assertIn("H", out)
        d = GestureDetector()
        out = []
        for _ in range(10):         # and the other way round
            out += d.update(hand(0.5, 0.5, grip=1.0), t, world=self.world(0.0))
            t += 1 / FPS
        self.assertNotIn("H", out)

    def test_the_pointer_still_comes_from_the_picture(self):
        d = GestureDetector()
        for i in range(15):
            out = d.update(hand(0.5 + STEP, 0.5), i / FPS, world=self.world(0.0))
        self.assertGreater(int(out[0][1:].split("Y")[0]), 750)


if __name__ == "__main__":
    unittest.main()
