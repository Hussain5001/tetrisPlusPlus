#!/usr/bin/env python3
"""End-to-end smoke test for Tetris++.

Starts the real game, plays it by sending gesture commands over UDP (the same
way the camera script does), and checks what it saved. Needs a display; on a
server run it with a virtual one:

    xvfb-run -a python3 tests/smoke_test.py build/Tetris

What it covers:
  1. boot screen -> main menu -> Time Attack -> countdown -> hard drops until
     game over -> the result is saved in scores.json (last run + top 5)
  2. game over menu -> main menu -> Zen setup -> change a setting (saved in
     profile.json) -> start Zen -> pause -> "save & quit" writes a save file
     and the game exits cleanly
  3. second run: Zen setup -> "continue saved" loads it -> pause -> quit
"""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

PORT = 5005


class Game:
    def __init__(self, exe, workdir, name):
        self.log_path = os.path.join(workdir, name + ".log")
        self.log = open(self.log_path, "w")
        self.proc = subprocess.Popen([exe], cwd=workdir, stdout=self.log,
                                     stderr=subprocess.STDOUT)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def output(self):
        with open(self.log_path, errors="ignore") as f:
            return f.read()

    def wait_for(self, text, timeout, what):
        end = time.time() + timeout
        while time.time() < end:
            if text in self.output():
                return
            if self.proc.poll() is not None:
                fail(f"game exited while waiting for {what}", self)
            time.sleep(0.1)
        fail(f"timed out waiting for {what}", self)

    def send(self, *commands, gap=0.25):
        for c in commands:
            self.sock.sendto(c.encode(), ("127.0.0.1", PORT))
            time.sleep(gap)

    def wait_exit(self, timeout, what):
        try:
            code = self.proc.wait(timeout)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            fail(f"game didn't exit after {what}", self)
        if code != 0:
            fail(f"game exited with code {code} after {what}", self)
        self.log.close()


def fail(message, game=None):
    print("SMOKE TEST FAILED:", message)
    if game:
        print("---- game output (last 40 lines) ----")
        print("\n".join(game.output().splitlines()[-40:]))
        if game.proc.poll() is None:
            game.proc.kill()
    sys.exit(1)


def ok(message):
    print("ok -", message)


def load_json(path, what):
    if not os.path.exists(path):
        fail(f"{what} was not written ({path})")
    with open(path) as f:
        return json.load(f)


def main():
    exe = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "build/Tetris")
    workdir = tempfile.mkdtemp(prefix="tetris_smoke_")
    print("working in", workdir)

    # ---- run 1
    game = Game(exe, workdir, "run1")
    game.wait_for("listening on udp://127.0.0.1:5005", 20, "the game to start")
    time.sleep(1.0)
    ok("game started and listens for gestures")

    game.send("C", gap=0.6)          # skip the boot screen
    game.send("R", "C", gap=0.4)     # main menu: time attack
    time.sleep(2.2)                  # 3-2-1 countdown
    game.wait_for("constructed grid", 5, "a game to start")
    for _ in range(60):              # hard drop until the board is full
        if "Game Over" in game.output():
            break
        game.send("H", gap=0.35)
    game.wait_for("Game Over", 5, "game over")
    time.sleep(1.0)
    scores = load_json(os.path.join(workdir, "scores.json"), "scores.json")
    last = scores.get("time_attack", {}).get("last")
    top = scores.get("time_attack", {}).get("top", [])
    if not last or last["value"] < 20 or not top or top[0]["value"] != last["value"]:
        fail(f"unexpected time attack scores: {scores}", game)
    ok(f"time attack game over saved: score {last['value']:.0f}, rank 1")

    game.send("R", "C", gap=0.5)     # game over menu: main menu
    game.send("L", "C", gap=0.6)     # main menu remembers "time attack"; back to zen -> setup
    game.send("U", "R", gap=0.4)     # up to "sound", turn it off
    profile = load_json(os.path.join(workdir, "profile.json"), "profile.json")
    if profile.get("sound") is not False:
        fail(f"sound setting not saved: {profile}", game)
    ok("zen setup change saved to profile.json")

    game.send("D", "C", gap=0.4)     # down to "start", start
    time.sleep(2.2)                  # countdown
    game.send("H", "H", gap=0.4)
    game.send("P", gap=0.6)          # open palm = pause
    game.send("D", "H", gap=0.4)     # pause menu: save & quit (a drop selects)
    game.wait_exit(10, "save & quit")
    save = load_json(os.path.join(workdir, "game_state.json"), "game_state.json")
    if save.get("level") != 5 or save.get("lines_per_level") != 5 or "game_grid" not in save:
        fail(f"unexpected save file: {save}")
    filled = sum(1 for row in save["game_grid"] for cell in row if cell)
    if filled < 8:
        fail(f"save file has only {filled} filled cells after two hard drops")
    ok(f"zen save & quit wrote game_state.json ({filled} filled cells, level 5)")

    # ---- run 2
    game = Game(exe, workdir, "run2")
    game.wait_for("listening on udp://127.0.0.1:5005", 20, "the game to restart")
    time.sleep(1.0)
    game.send("C", gap=0.6)          # skip boot
    game.send("C", gap=0.6)          # zen setup
    game.send("D", "C", gap=0.4)     # "continue saved"
    game.wait_for("Reading from file", 5, "the saved game to load")
    ok("continue saved loads the game")
    time.sleep(2.0)
    game.send("C", gap=0.6)          # pause
    game.send("U", "C", gap=0.4)     # pause menu: up wraps to "quit"
    game.wait_exit(10, "quit")
    ok("quit from the pause menu exits cleanly")

    print("SMOKE TEST PASSED")


if __name__ == "__main__":
    main()
