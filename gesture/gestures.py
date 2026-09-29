"""Turns a stream of hand landmarks into Tetris commands.

This module has no camera or MediaPipe dependency, so the detection logic can
be unit tested with synthetic hand movements (see test_gestures.py).

Commands (one letter each, matching src/input/GestureSource.cpp):
    L / R  move left / right       H  hard drop     U  rotate
    P      pause                   C  select (palm / position modes)
    @N     move the piece to column N (position mode)

Modes:
    flick     (default) point your index finger up; swipe it left or right to
              move one column (bringing it back doesn't count), dip it down
              and straight again (or swipe the hand down) to drop
    point     point your index finger up; tilt it left/right and hold to keep
              moving (like holding an arrow key); drops like flick mode
    palm      the original open-hand swipes, a held fist pauses/selects
    position  the piece follows your hand sideways, a held fist pauses/selects
In every mode pinching (thumb to index tip) rotates. In the finger modes an
open palm held still for a second pauses; a fist or a relaxed hand does
nothing, so you can rest your hand any time. In menus: swipe to move, drop to
select, pinch to go up.
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

    # Index finger modes. Flick mode follows the fingertip on screen, so
    # tilting the finger and moving the whole hand both count as a swipe.
    swipe_distance: float = 0.35       # fingertip travel (hand sizes) for left/right
    drop_distance: float = 0.5         # fingertip travel down for a drop
    stroke_time: float = 0.3           # ...within this many seconds (so slow
                                       # drifts don't count)
    stroke_axis_ratio: float = 1.3     # a swipe must be mostly along one axis
    stroke_release: float = 1.0        # slower than this (hand sizes/s) ends a swipe
    return_margin: float = 0.25        # a return stroke ends this close (in
                                       # swipe distances) to where the swipe began
    tap_time: float = 0.6              # a dipped finger must point again this
                                       # fast to drop (a curl to rest doesn't)
    pinch_quiet: float = 0.25          # no moves just after a pinch
    palm_hold: float = 1.0             # hold an open palm still this long to pause
    tip_smoothing: float = 0.25        # 0 = none
    # Point mode: the finger's angle around its knuckle
    finger_threshold: float = 0.38     # radians (~22 deg) of tilt for left/right
    point_repeat_delay: float = 0.35   # first repeat after holding
    point_repeat_rate: float = 0.12    # then one move per this long
    baseline_time: float = 0.6         # how fast the resting pose is learned (s)
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
    zone: str = ""                  # "L"/"R" while tilted, "" once back at rest
    next_repeat: float = 0.0
    # Fingertip strokes (flick swipes and drops)
    tip: tuple | None = None        # smoothed fingertip position
    size: float | None = None       # smoothed hand size
    strokes: deque = field(default_factory=lambda: deque(maxlen=64))  # (t, x, y, pointing)
    blocked: str = ""               # direction that must slow down before it fires again
    return_dir: str = ""            # direction that is just bringing the finger back
    return_x: float = 0.0           # ...to where the last swipe started
    drop_blocked: bool = False
    dip_until: float | None = None  # finger dipped: drop if it points again by then
    straight_frames: int = 0
    pinch_time: float = -1e9
    not_pointing_since: float | None = None
    palm_since: float | None = None
    palm_ref: tuple = (0.0, 0.0)
    palm_fired: bool = False


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
        self.debug = {"pointing": False, "dx": 0.0, "threshold": self._meter_threshold()}

    def _meter_threshold(self):
        return self.cfg.finger_threshold if self.mode == "point" else self.cfg.swipe_distance

    def toggle_mode(self):
        """Cycles flick -> point -> palm -> position."""
        self.mode = MODES[(MODES.index(self.mode) + 1) % len(MODES)]
        self.reset()
        self.debug["threshold"] = self._meter_threshold()

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
        out += self._pinch(landmarks, hand_size, t)
        if self.mode in FINGER_MODES:
            self._update_pointing(landmarks, hand_size)
            out += self._palm_pause(landmarks, hand_size, px, py, t)
            out += self._strokes(landmarks, hand_size, t)
            if self.mode == "point":
                if st.pointing and not st.pinching and st.dip_until is None:
                    out += self._finger(landmarks, hand_size, t)
                else:
                    self._finger_idle(t)
        else:
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
        """Point mode, not pointing (or pinching / dipping): stop repeats."""
        st = self.state
        st.finger_t = t
        st.prev_angle = None

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
            elif held and t >= st.next_repeat:
                out.append(st.zone)
                st.next_repeat += cfg.point_repeat_rate
            elif tilted and tilted != st.zone:
                # swung straight across to the other side
                st.zone = tilted
                st.next_repeat = t + cfg.point_repeat_delay
                out.append(tilted)
        elif tilted:
            st.zone = tilted
            st.next_repeat = t + cfg.point_repeat_delay
            out.append(tilted)
        return out

    def _strokes(self, lm, hand_size, t):
        """Fingertip swipes: left/right (flick mode) and the drop (both
        finger modes). Positions are in image units; travel is measured in
        hand sizes so it doesn't matter how far you sit from the camera."""
        cfg, st = self.cfg, self.state
        raw = self.is_pointing(lm, hand_size)
        tip = lm[INDEX_TIP]
        a = cfg.tip_smoothing
        if st.tip is None:
            st.tip, st.size = tip, hand_size
        else:
            st.tip = (a * st.tip[0] + (1 - a) * tip[0], a * st.tip[1] + (1 - a) * tip[1])
            st.size = 0.8 * st.size + 0.2 * hand_size
        size, (tx, ty) = st.size, st.tip

        # Pinching moves the index tip too: that's a rotate, never a move
        if st.pinching or t - st.pinch_time < cfg.pinch_quiet:
            st.strokes.clear()
            st.dip_until = None
            return []

        # A hand that stopped pointing for a while has been put away / moved
        # somewhere else: forget the last swipe
        if st.pointing:
            st.not_pointing_since = None
        elif st.not_pointing_since is None:
            st.not_pointing_since = t
        elif t - st.not_pointing_since > 0.4:
            st.return_dir = ""
            st.blocked = ""

        st.strokes.append((t, tx, ty, raw))
        while t - st.strokes[0][0] > cfg.stroke_time:
            st.strokes.popleft()

        # Recent velocity, to tell when a swipe has stopped
        vx = vy = 0.0
        recent = [s for s in st.strokes if t - s[0] <= 0.1]
        if len(recent) >= 2 and recent[-1][0] - recent[0][0] >= 0.05:
            span = recent[-1][0] - recent[0][0]
            vx = (recent[-1][1] - recent[0][1]) / span / size
            vy = (recent[-1][2] - recent[0][2]) / span / size
            stopped = False
            if (st.blocked == "L" and vx > -cfg.stroke_release) or \
                    (st.blocked == "R" and vx < cfg.stroke_release):
                st.blocked, stopped = "", True
            if st.drop_blocked and vy < cfg.stroke_release:
                st.drop_blocked, stopped = False, True
            if stopped:
                # the swipe is over: measure the next one from here
                st.strokes = deque([st.strokes[-1]], maxlen=st.strokes.maxlen)

        out = []
        if cfg.vertical_swipes:
            out += self._drop(raw, tx, ty, size, t)
        if self.mode == "flick" and st.pointing and st.dip_until is None and not out:
            out += self._flick(tx, ty, size, t)
        return out

    def _drop(self, raw, tx, ty, size, t):
        """The fingertip moves down fast. If the finger is still pointing
        (the whole hand moved) that's a drop straight away. If the finger
        bent (a tap), it's a drop once it points again, so curling it to
        rest never drops."""
        cfg, st = self.cfg, self.state
        if st.dip_until is not None:
            st.straight_frames = st.straight_frames + 1 if raw else 0
            if st.straight_frames >= 2:
                st.dip_until = None
                st.strokes.clear()
                st.drop_blocked = True
                return ["H"]
            if t > st.dip_until:
                st.dip_until = None  # it stayed curled: resting, not a tap
                st.strokes.clear()
            return []
        if st.drop_blocked:
            return []
        pointed = [s for s in st.strokes if s[3]]
        if not pointed:
            return []
        top = min(pointed, key=lambda s: s[2])
        dy = (ty - top[2]) / size
        dx = (tx - top[1]) / size
        if dy < cfg.drop_distance or dy < cfg.stroke_axis_ratio * abs(dx):
            return []
        st.strokes.clear()
        if raw and st.pointing:
            st.drop_blocked = True
            return ["H"]
        st.dip_until = t + cfg.tap_time
        st.straight_frames = 0
        return []

    def _flick(self, tx, ty, size, t):
        """Left/right swipes of the fingertip. After a swipe, the movement
        back to where it started is ignored; carry on past that point to
        swipe the other way."""
        cfg, st = self.cfg, self.state
        D = cfg.swipe_distance
        if st.return_dir:
            back = (tx - st.return_x) / size
            if (back >= -cfg.return_margin * D if st.return_dir == "R"
                    else back <= cfg.return_margin * D):
                # back where the last swipe started: a new stroke starts here
                st.return_dir = ""
                st.strokes = deque([st.strokes[-1]], maxlen=st.strokes.maxlen)

        lo = min(st.strokes, key=lambda s: s[1])
        hi = max(st.strokes, key=lambda s: s[1])
        right = (tx - lo[1]) / size
        left = (hi[1] - tx) / size
        self.debug["dx"] = right if right >= left else -left
        self.debug["threshold"] = D
        direction, origin = "", None
        if right >= D and right >= cfg.stroke_axis_ratio * abs(ty - lo[2]) / size:
            direction, origin = "R", lo
        elif left >= D and left >= cfg.stroke_axis_ratio * abs(ty - hi[2]) / size:
            direction, origin = "L", hi
        if not direction or direction == st.blocked or direction == st.return_dir:
            return []
        if not st.return_dir:
            st.return_x = origin[1]  # another swipe the same way keeps the first start
        st.return_dir = "L" if direction == "R" else "R"
        st.blocked = direction
        st.strokes = deque([st.strokes[-1]], maxlen=st.strokes.maxlen)
        return [direction]

    @staticmethod
    def is_open_palm(lm, hand_size=None):
        """All four fingers stretched out and the thumb away from the hand."""
        wrist = lm[WRIST]
        if hand_size is None:
            hand_size = max(_dist(wrist, lm[MIDDLE_MCP]), 1e-3)
        for tip, pip in ((INDEX_TIP, INDEX_PIP), (MIDDLE_TIP, MIDDLE_PIP),
                         (RING_TIP, RING_PIP), (PINKY_TIP, PINKY_PIP)):
            if _dist(lm[tip], wrist) < 1.1 * _dist(lm[pip], wrist):
                return False
        return _dist(lm[THUMB_TIP], lm[INDEX_MCP]) > 0.5 * hand_size

    def _palm_pause(self, lm, hand_size, px, py, t):
        """Finger modes: an open palm held still ("stop") pauses once."""
        st = self.state
        if st.pinching or not self.is_open_palm(lm, hand_size):
            st.palm_since = None
            st.palm_fired = False
            return []
        if st.palm_since is None or \
                _dist((px, py), st.palm_ref) / hand_size > 0.5:
            st.palm_since, st.palm_ref = t, (px, py)  # (re)start when it moves
        if not st.palm_fired and t - st.palm_since >= self.cfg.palm_hold:
            st.palm_fired = True
            return ["P"]
        return []

    def _pinch(self, lm, hand_size, t):
        st = self.state
        d = _dist(lm[THUMB_TIP], lm[INDEX_TIP]) / hand_size
        if st.pinching:
            st.pinch_time = t
        if not st.pinching and d < self.cfg.pinch_on:
            st.pinching = True
            st.pinch_time = t
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
