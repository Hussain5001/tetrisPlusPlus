"""Turns a stream of hand landmarks into Tetris commands.

This module has no camera or MediaPipe dependency, so the detection logic can
be unit tested with synthetic hand movements (see test_gestures.py).

Commands (one letter each, matching src/input/GestureSource.cpp):
    L / R  move left / right       H  hard drop     U  rotate
    C      pause / select          @N move the piece to column N (position mode)
"""
from __future__ import annotations

import math
from collections import deque
from dataclasses import dataclass, field

# MediaPipe hand landmark indices
WRIST = 0
THUMB_TIP = 4
INDEX_MCP, INDEX_PIP, INDEX_TIP = 5, 6, 8
MIDDLE_MCP, MIDDLE_PIP, MIDDLE_TIP = 9, 10, 12
RING_MCP, RING_PIP, RING_TIP = 13, 14, 16
PINKY_MCP, PINKY_PIP, PINKY_TIP = 17, 18, 20
PALM = (WRIST, INDEX_MCP, MIDDLE_MCP, RING_MCP, PINKY_MCP)


@dataclass
class Config:
    # Speeds are in "hand sizes per second" (wrist -> middle knuckle), so the
    # same swipe works whether you sit close to the camera or far away.
    swipe_fire_speed: float = 4.5      # speed that triggers a swipe
    swipe_release_speed: float = 2.0   # must slow below this to re-arm
    swipe_axis_ratio: float = 1.4      # dominant axis must be this much larger
    velocity_window: float = 0.07      # seconds of history used for velocity
    swipe_cooldown: float = 0.12       # min seconds between two swipes
    return_suppress: float = 0.25      # ignore the opposite swipe for this long
                                       # after the previous swipe has stopped
    smoothing: float = 0.6             # 0 = none, closer to 1 = smoother/slower
    pinch_on: float = 0.28             # thumb-index distance (hand sizes)
    pinch_off: float = 0.45
    fist_hold: float = 0.45            # seconds to hold a fist for pause/select
    columns: int = 10                  # board width for position mode
    position_margin: float = 0.2       # ignore the outer 20% of the frame
    vertical_swipes: bool = True       # swipe down = drop, swipe up = rotate


@dataclass
class HandState:
    history: deque = field(default_factory=lambda: deque(maxlen=64))
    smoothed: tuple | None = None
    armed: bool = True
    last_swipe_time: float = -1e9
    last_swipe_dir: str = ""
    swipe_end_time: float = -1e9
    pinching: bool = False
    fist_since: float | None = None
    fist_fired: bool = False


def _dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


class GestureDetector:
    """Feed it landmarks every frame; it returns the commands to send."""

    def __init__(self, config: Config | None = None, mode: str = "swipe"):
        self.cfg = config or Config()
        self.mode = mode  # "swipe" or "position"
        self.state = HandState()
        self.last_label = ""
        self.last_column: int | None = None

    def toggle_mode(self):
        self.mode = "position" if self.mode == "swipe" else "swipe"
        self.reset()

    def reset(self):
        self.state = HandState()
        self.last_column = None

    def update(self, landmarks, t: float) -> list[str]:
        """landmarks: 21 (x, y) pairs in 0..1 image coordinates (already
        mirrored so moving your hand right moves x up), or None if no hand.
        t: timestamp in seconds. Returns a list of command strings."""
        if landmarks is None:
            self.reset()
            return []

        cfg, st = self.cfg, self.state
        hand_size = max(_dist(landmarks[WRIST], landmarks[MIDDLE_MCP]), 1e-3)
        px = sum(landmarks[i][0] for i in PALM) / len(PALM)
        py = sum(landmarks[i][1] for i in PALM) / len(PALM)

        # Light exponential smoothing removes landmark jitter
        if st.smoothed is None:
            st.smoothed = (px, py)
        else:
            a = cfg.smoothing
            st.smoothed = (a * st.smoothed[0] + (1 - a) * px,
                           a * st.smoothed[1] + (1 - a) * py)
        st.history.append((t, st.smoothed[0], st.smoothed[1], hand_size))

        out: list[str] = []
        out += self._pinch(landmarks, hand_size)
        out += self._fist(landmarks, t)
        if st.fist_since is None:  # don't swipe while making a fist
            out += self._swipe(t)
        if self.mode == "position":
            out += self._position(px)
        for c in out:
            if not c.startswith("@"):
                self.last_label = c
        return out

    # ------------------------------------------------------------ gestures

    def _velocity(self, t):
        """Velocity over the last `velocity_window` seconds, in hand sizes/s."""
        hist = self.state.history
        if len(hist) < 2:
            return 0.0, 0.0
        newest = hist[-1]
        oldest = newest
        for sample in reversed(hist):
            oldest = sample
            if newest[0] - sample[0] >= self.cfg.velocity_window:
                break
        dt = newest[0] - oldest[0]
        if dt <= 0:
            return 0.0, 0.0
        size = newest[3]
        return ((newest[1] - oldest[1]) / dt / size,
                (newest[2] - oldest[2]) / dt / size)

    def _swipe(self, t):
        cfg, st = self.cfg, self.state
        vx, vy = self._velocity(t)
        speed = math.hypot(vx, vy)

        if not st.armed:
            # Wait for the hand to slow down before the next swipe, so one
            # long movement only counts once
            if speed < cfg.swipe_release_speed:
                st.armed = True
                st.swipe_end_time = t
            return []
        # Fire as soon as the speed crosses the threshold (early in the
        # movement) rather than waiting for the swipe to finish
        if speed < cfg.swipe_fire_speed or t - st.last_swipe_time < cfg.swipe_cooldown:
            return []

        if abs(vx) >= cfg.swipe_axis_ratio * abs(vy):
            direction = "R" if vx > 0 else "L"
            if self.mode == "position":
                return []  # horizontal movement is handled by position mode
        elif cfg.vertical_swipes and abs(vy) >= cfg.swipe_axis_ratio * abs(vx):
            direction = "H" if vy > 0 else "U"  # image y grows downwards
        else:
            return []  # diagonal, ambiguous

        # Bringing your hand back after a swipe is not a swipe
        opposite = {"L": "R", "R": "L", "H": "U", "U": "H"}
        if (st.last_swipe_dir == opposite[direction]
                and t - st.swipe_end_time < cfg.return_suppress):
            st.armed = False
            return []

        st.armed = False
        st.last_swipe_time = t
        st.last_swipe_dir = direction
        return [direction]

    def _pinch(self, lm, hand_size):
        st = self.state
        d = _dist(lm[THUMB_TIP], lm[INDEX_TIP]) / hand_size
        if not st.pinching and d < self.cfg.pinch_on:
            st.pinching = True
            return ["U"]
        if st.pinching and d > self.cfg.pinch_off:
            st.pinching = False
        return []

    @staticmethod
    def is_fist(lm):
        """All four fingertips closer to the wrist than their middle joints."""
        wrist = lm[WRIST]
        for tip, pip in ((INDEX_TIP, INDEX_PIP), (MIDDLE_TIP, MIDDLE_PIP),
                         (RING_TIP, RING_PIP), (PINKY_TIP, PINKY_PIP)):
            if _dist(lm[tip], wrist) > _dist(lm[pip], wrist):
                return False
        return True

    def _fist(self, lm, t):
        st = self.state
        if not self.is_fist(lm):
            st.fist_since = None
            st.fist_fired = False
            return []
        if st.fist_since is None:
            st.fist_since = t
        if not st.fist_fired and t - st.fist_since >= self.cfg.fist_hold:
            st.fist_fired = True
            return ["C"]
        return []

    def _position(self, palm_x):
        """Map the palm's x position to a board column."""
        m = self.cfg.position_margin
        u = min(max((palm_x - m) / (1 - 2 * m), 0.0), 0.999)
        column = int(u * self.cfg.columns)
        # Small hysteresis so the piece doesn't flicker between two columns
        if self.last_column is not None and column != self.last_column:
            centre = (self.last_column + 0.5) / self.cfg.columns
            if abs(u - centre) < 0.8 / self.cfg.columns:
                column = self.last_column
        self.last_column = column
        return [f"@{column}"]
