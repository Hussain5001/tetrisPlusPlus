#!/usr/bin/env python3
"""Runs the gesture detector over a recording, without a camera.

Record yourself once, then try detector settings on exactly the same hand
movements as often as you like:

    py gesture/hand_control.py --record pinches.jsonl     (do 20 pinches, press q)
    py gesture/replay.py pinches.jsonl --expect U=20
    py gesture/replay.py pinches.jsonl --expect U=20 --set pinch_on=0.32

--expect says what you really did, so the report can count missed and extra
commands; every command you did not list counts as false. --set changes one
value of gestures.Config (repeat it for several). --mode replays in another
mode than the one recorded. --timeline also lists every command with its time.
"""
from __future__ import annotations

import argparse
import dataclasses
import json
import os
import sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gestures import MODE_ALIASES, MODES, Config, GestureDetector  # noqa: E402

ORDER = "LRUHDPCB"  # the order commands are listed in


def load(path):
    """Reads a recording. Returns (header, frames); each frame is a dict with
    "t", "lm" (21 [x, y, z] or None when no hand was seen) and "cmd"."""
    with open(path, encoding="utf-8") as f:
        lines = [line for line in f if line.strip()]
    if not lines:
        raise ValueError(f"{path} is empty")
    header = json.loads(lines[0])
    if "tetris_gesture_recording" not in header:
        raise ValueError(f"{path} is not a hand_control.py recording")
    return header, [json.loads(line) for line in lines[1:]]


def make_config(header, overrides=()):
    """The recorded Config with "name=value" overrides applied. Values the
    current Config doesn't know (an old recording) are ignored."""
    cfg = Config()
    names = {f.name: f.type for f in dataclasses.fields(Config)}
    for name, value in header.get("config", {}).items():
        if name in names:
            setattr(cfg, name, value)
    for item in overrides:
        name, sep, text = item.partition("=")
        if not sep or name not in names:
            raise ValueError(f"unknown setting {item!r}; see Config in gestures.py")
        current = getattr(cfg, name)
        if isinstance(current, bool):
            value = text.lower() in ("1", "true", "yes", "on")
        else:
            value = type(current)(text)
        setattr(cfg, name, value)
    return cfg


def replay(frames, detector):
    """Feeds every frame to the detector. Returns [(time, command), ...]
    (position targets like "@4" are left out)."""
    out = []
    for frame in frames:
        lm = frame.get("lm")
        landmarks = [(p[0], p[1]) for p in lm] if lm else None
        for command in detector.update(landmarks, frame["t"]):
            if not command.startswith("@"):
                out.append((frame["t"], command))
    return out


def recorded_commands(frames):
    return [c for frame in frames for c in frame.get("cmd", []) if not c.startswith("@")]


def parse_expect(text):
    """"U=20,H=0" -> {"U": 20, "H": 0}"""
    expected = {}
    for part in filter(None, (p.strip() for p in (text or "").split(","))):
        letter, sep, count = part.partition("=")
        if not sep or len(letter) != 1 or not count.isdigit():
            raise ValueError(f"bad --expect item {part!r}, use e.g. U=20,H=0")
        expected[letter.upper()] = int(count)
    return expected


def score(counts, expected):
    """Compares what the detector sent with what the player really did.
    Returns {"per_command": {letter: (expected, got, missed, extra)},
    "false": {letter: count of commands that were not expected at all}}."""
    per_command = {}
    for letter, want in expected.items():
        got = counts.get(letter, 0)
        per_command[letter] = (want, got, max(0, want - got), max(0, got - want))
    false = {c: n for c, n in counts.items() if c not in expected}
    return {"per_command": per_command, "false": false}


def format_counts(counts):
    letters = sorted(counts, key=lambda c: (ORDER.find(c) if c in ORDER else 99, c))
    return ", ".join(f"{c} {counts[c]}" for c in letters) or "none"


def report(header, frames, events, mode, expected=None, timeline=False):
    """The text replay.py prints."""
    lines = []
    seen = sum(1 for f in frames if f.get("lm"))
    length = frames[-1]["t"] - frames[0]["t"] if frames else 0.0
    share = 100 * seen / len(frames) if frames else 0
    lines.append(f"recording: {len(frames)} frames, {length:.1f} s, "
                 f"hand seen in {seen} ({share:.0f}%)")
    lines.append(f"recorded in {header.get('mode', '?')} mode, replayed in {mode} mode")
    counts = Counter(c for _, c in events)
    lines.append(f"commands: {format_counts(counts)}")
    then = recorded_commands(frames)
    same = [c for _, c in events] == then
    lines.append("same as when it was recorded" if same
                 else f"differs from the recording, which had: {format_counts(Counter(then))}")
    if expected:
        result = score(counts, expected)
        for letter, (want, got, missed, extra) in result["per_command"].items():
            lines.append(f"expected {letter} x{want}: got {got} "
                         f"(missed {missed}, extra {extra})")
        lines.append(f"false commands: {format_counts(result['false'])}")
    if timeline:
        lines += [f"  {t:7.2f}s  {c}" for t, c in events]
    return "\n".join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("recording", help="file made with hand_control.py --record")
    ap.add_argument("--mode", choices=list(MODES) + list(MODE_ALIASES),
                    help="detector mode (default: the recorded one)")
    ap.add_argument("--set", action="append", default=[], metavar="NAME=VALUE",
                    help="change a Config value, e.g. --set pinch_on=0.32")
    ap.add_argument("--expect", metavar="U=20,H=0",
                    help="what you really did, to count missed and extra commands")
    ap.add_argument("--timeline", action="store_true", help="list every command")
    args = ap.parse_args(argv)

    try:
        header, frames = load(args.recording)
        cfg = make_config(header, args.set)
        expected = parse_expect(args.expect)
    except (OSError, ValueError) as err:
        sys.exit(f"replay.py: {err}")
    mode = args.mode or header.get("mode", "flick")
    detector = GestureDetector(cfg, mode)
    events = replay(frames, detector)
    print(report(header, frames, events, detector.mode, expected, args.timeline))


if __name__ == "__main__":
    main()
