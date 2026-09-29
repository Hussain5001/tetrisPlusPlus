"""Turns a stream of hand landmarks into Tetris commands.

This module has no camera or MediaPipe dependency, so the detection logic can
be unit tested with synthetic hand movements (see test_gestures.py).

Commands (one letter each, matching src/input/GestureSource.cpp):
    L / R  move left / right       H  hard drop     U  rotate
    C      pause / select          @N move the piece to column N (position mode)

Modes:
    flick     (default) point with your index finger; flick the finger left or
              right to move one column, flick the fingertip down to drop
    point     point with your index finger; tilt it left/right and hold to keep
              moving (like holding an arrow key)
    palm      the original open-hand swipes
    position  the piece follows your hand sideways
In every mode: pinch (thumb to index tip) rotates, a held fist pauses/selects.
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

MODES = ("flick", "point", "palm", "position")
FINGER_MODES = ("flick", "point")
MODE_ALIASES = {"swipe": "palm"}  # the old name of palm mode


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

    # Index finger modes. Left/right is the finger's angle around its
    # knuckle, so moving the whole hand doesn't count.
    finger_threshold: float = 0.38     # radians (~22 deg) of tilt for left/right
    finger_axis_ratio: float = 1.2     # a drop must be mostly downwards
    flick_time: float = 0.25           # a flick reaches the threshold this fast
    point_repeat_delay: float = 0.35   # point mode: first repeat after holding
    point_repeat_rate: float = 0.12    # point mode: then one move per this long
    baseline_time: float = 0.6         # how fast the resting pose is learned (s)
    drop_speed: float = 3.5            # hand speed down for a drop flick
    drop_window: float = 0.1           # seconds of history for that speed
    max_gap_frames: int = 4            # frames the hand may vanish (blur)


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
    missed_frames: int = 0
    # Index finger modes
    pointing: bool = False
    pose_count: int = 0             # frames the pose has disagreed with `pointing`
    baseline: float | None = None   # resting finger angle (radians)
    prev_angle: float | None = None
    finger_t: float | None = None
    rest_time: float = -1e9         # last time the finger was near rest
    zone: str = ""                  # "L"/"R" while tilted, "" once back at rest
    next_repeat: float = 0.0
    tip_history: deque = field(default_factory=lambda: deque(maxlen=64))
    drop_armed: bool = True


def _dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


class GestureDetector:
    """Feed it landmarks every frame; it returns the commands to send."""

    def __init__(self, config: Config | None = None, mode: str = "flick"):
        self.cfg = config or Config()
        mode = MODE_ALIASES.get(mode, mode)
        if mode not in MODES:
            raise ValueError(f"unknown mode {mode!r}, use one of {MODES}")
        self.mode = mode
        self.state = HandState()
        self.last_label = ""
        self.last_column: int | None = None
        # What the finger modes see, for the camera preview
        self.debug = {"pointing": False, "dx": 0.0, "threshold": self.cfg.finger_threshold}

    def toggle_mode(self):
        """Cycles flick -> point -> palm -> position."""
        self.mode = MODES[(MODES.index(self.mode) + 1) % len(MODES)]
        self.reset()

    def reset(self):
        self.state = HandState()
        self.last_column = None

    def update(self, landmarks, t: float) -> list[str]:
        """landmarks: 21 (x, y) pairs in 0..1 image coordinates (already
        mirrored so moving your hand right moves x up), or None if no hand.
        t: timestamp in seconds. Returns a list of command strings."""
        if landmarks is None:
            # A fast movement can blur the hand for a frame or two; only
            # forget the movement if the hand is really gone
            self.state.missed_frames += 1
            if self.state.missed_frames > self.cfg.max_gap_frames:
                self.reset()
            return []
        self.state.missed_frames = 0

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
        if self.mode in FINGER_MODES:
            self._update_pointing(landmarks, hand_size)
            if st.pointing and not st.pinching and st.fist_since is None:
                out += self._finger(landmarks, hand_size, t)
                if cfg.vertical_swipes:
                    out += self._drop(landmarks, hand_size, t)
            else:
                self._finger_idle(t)
        elif st.fist_since is None:  # don't swipe while making a fist
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

    # ------------------------------------------------------------ index finger

    @staticmethod
    def is_pointing(lm, hand_size=None):
        """Index finger stretched out, the other three fingers curled in
        (the thumb can do anything)."""
        wrist = lm[WRIST]
        if hand_size is None:
            hand_size = max(_dist(wrist, lm[MIDDLE_MCP]), 1e-3)
        index_out = (_dist(lm[INDEX_TIP], wrist) > 1.05 * _dist(lm[INDEX_PIP], wrist)
                     and _dist(lm[INDEX_TIP], lm[INDEX_MCP]) > 0.45 * hand_size)
        if not index_out:
            return False
        for tip, pip in ((MIDDLE_TIP, MIDDLE_PIP), (RING_TIP, RING_PIP),
                         (PINKY_TIP, PINKY_PIP)):
            if _dist(lm[tip], wrist) > _dist(lm[pip], wrist):
                return False
        return True

    def _update_pointing(self, lm, hand_size):
        """Two frames in a row must agree before the pose changes."""
        st = self.state
        now = self.is_pointing(lm, hand_size)
        if now == st.pointing:
            st.pose_count = 0
        else:
            st.pose_count += 1
            if st.pose_count >= 2:
                st.pointing = now
                st.pose_count = 0
        self.debug["pointing"] = st.pointing

    def _finger_idle(self, t):
        """Not pointing (or pinching / fist): stop repeats; the next tilt
        needs the finger back at rest first."""
        st = self.state
        st.finger_t = t
        st.prev_angle = None
        st.tip_history.clear()

    @staticmethod
    def finger_angle(lm):
        """Angle of the index finger around its knuckle: 0 = straight up,
        positive = leaning right on screen."""
        return math.atan2(lm[INDEX_TIP][0] - lm[INDEX_MCP][0],
                          lm[INDEX_MCP][1] - lm[INDEX_TIP][1])

    def _finger(self, lm, hand_size, t):
        cfg, st = self.cfg, self.state
        angle = self.finger_angle(lm)
        dt = 0.0 if st.finger_t is None else max(t - st.finger_t, 0.0)
        st.finger_t = t
        if st.baseline is None:
            st.baseline = angle
            st.rest_time = t
            return []

        T = cfg.finger_threshold
        d = math.remainder(angle - st.baseline, math.tau)
        self.debug["dx"] = d
        self.debug["threshold"] = T

        # Learn the resting angle slowly: near rest, or held still at a small
        # tilt (people's "straight" finger drifts during a game)
        speed = 0.0
        if st.prev_angle is not None and dt > 0:
            speed = abs(math.remainder(angle - st.prev_angle, math.tau)) / dt
        st.prev_angle = angle
        if abs(d) < T / 3:
            st.rest_time = t
        learn_time = None
        if abs(d) < T / 3:
            learn_time = cfg.baseline_time
        elif not st.zone and abs(d) < 0.8 * T and speed < 0.25:
            learn_time = 3 * cfg.baseline_time  # slower away from rest
        if learn_time and dt > 0:
            st.baseline += min(1.0, dt / learn_time) * d

        tilted = ""
        if abs(d) >= T:
            tilted = "R" if d > 0 else "L"

        out = []
        if st.zone:
            held = (d > 0) == (st.zone == "R") and abs(d) >= 0.6 * T
            if abs(d) < T / 2:
                st.zone = ""  # back at rest: ready for the next move
            elif self.mode == "point":
                if held and t >= st.next_repeat:
                    out.append(st.zone)
                    st.next_repeat += cfg.point_repeat_rate
                elif tilted and tilted != st.zone:
                    # swung straight across to the other side
                    st.zone = tilted
                    st.next_repeat = t + cfg.point_repeat_delay
                    out.append(tilted)
        elif tilted:
            st.zone = tilted
            if self.mode == "point":
                st.next_repeat = t + cfg.point_repeat_delay
                out.append(tilted)
            elif t - st.rest_time <= cfg.flick_time:
                out.append(tilted)  # a quick flick; slow drifts don't count
        return out

    def _drop(self, lm, hand_size, t):
        """A quick downward flick of the pointing hand drops the piece. It
        follows the index knuckle, not the fingertip, so curling the finger
        into a fist doesn't count as a drop."""
        cfg, st = self.cfg, self.state
        if not self.is_pointing(lm, hand_size):
            st.tip_history.clear()
            return []
        st.tip_history.append((t, lm[INDEX_MCP][0], lm[INDEX_MCP][1], hand_size))
        newest = st.tip_history[-1]
        oldest = newest
        for sample in reversed(st.tip_history):
            oldest = sample
            if newest[0] - sample[0] >= cfg.drop_window:
                break
        span = newest[0] - oldest[0]
        if span <= 0:
            return []
        vx = (newest[1] - oldest[1]) / span / hand_size
        vy = (newest[2] - oldest[2]) / span / hand_size
        if not st.drop_armed:
            if math.hypot(vx, vy) < 0.4 * cfg.drop_speed:
                st.drop_armed = True
            return []
        if vy >= cfg.drop_speed and vy >= cfg.finger_axis_ratio * abs(vx):
            st.drop_armed = False
            st.zone = ""
            return ["H"]
        return []

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
