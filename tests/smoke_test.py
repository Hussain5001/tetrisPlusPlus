#!/usr/bin/env python3
"""End-to-end smoke test for Tetris++.

Starts the real game, plays it by sending gesture commands over UDP (the same
way the camera script does), and checks what it saved. It opens the real game
window and needs UDP port 5005 to be free (close the game first):

    py tests/smoke_test.py build/Release/Tetris.exe

CI runs it on Linux with a virtual display (xvfb-run).

What it covers:
  1. boot screen -> main menu -> hand control on: the game starts the camera
     script (a stand-in, tests/fake_sidecar.py, with no camera) and answers
     it -> hand control off: the script is told to close; both are saved in
     profile.json
  2. Time Attack -> countdown -> hard drops until game over -> the result is
     saved in scores.json (last run + top 5)
  3. game over menu -> main menu -> Zen setup -> change a setting (saved in
     profile.json) -> start Zen -> play by hand: a drop lands in the column
     under the hand, at the left and the right wall -> the hand leaves: the
     game pauses by itself -> "save & quit" writes a save file and the game
     exits cleanly
  4. second run with hand control saved as on: the camera script starts with
     the game -> Zen setup -> "continue saved" loads it -> pause -> quit: the
     script is told to close
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
        # Instead of the real camera script the game starts a stand-in
        self.sidecar_log = os.path.join(workdir, name + "_sidecar.log")
        fake = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_sidecar.py")
        env = dict(os.environ, TETRIS_SIDECAR_CMD=
                   f'"{sys.executable}" "{fake}" {PORT} "{self.sidecar_log}"')
        self.proc = subprocess.Popen([exe], cwd=workdir, stdout=self.log,
                                     stderr=subprocess.STDOUT, env=env)
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

    def sidecar_said(self):
        """What the stand-in camera script wrote down, one word per line."""
        if not os.path.exists(self.sidecar_log):
            return []
        with open(self.sidecar_log) as f:
            return f.read().split()

    def wait_for_sidecar(self, words, timeout, what):
        end = time.time() + timeout
        while time.time() < end:
            if self.sidecar_said() == words:
                return
            time.sleep(0.1)
        fail(f"timed out waiting for {what}; the camera script wrote "
             f"{self.sidecar_said()}, expected {words}", self)

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

    # ---- hand control on and off from the main menu
    profile_path = os.path.join(workdir, "profile.json")
    game.send("R", "R", "R", gap=0.4)   # main menu: down to "hand control: off"
    game.send("C", gap=0.4)             # switch it on
    game.wait_for("hand: connected", 15, "the camera script to connect")
    game.wait_for_sidecar(["started", "answered"], 5, "the game to answer the camera script")
    if load_json(profile_path, "profile.json").get("hand_control") is not True:
        fail("hand control on was not saved to profile.json", game)
    ok("hand control on: the game starts the camera script and answers it")
    game.send("C", gap=0.4)             # switch it off again
    game.wait_for_sidecar(["started", "answered", "quit"], 5,
                          "the camera script to be told to close")
    game.wait_for("hand: stopped", 5, "the game to notice the script has closed")
    if load_json(profile_path, "profile.json").get("hand_control") is not False:
        fail("hand control off was not saved to profile.json", game)
    ok("hand control off: the camera script is told to close")

    game.send("L", "L", "C", gap=0.4)   # main menu: back up to time attack
    time.sleep(2.2)                  # 3-2-1 countdown
    game.wait_for("game: started time attack", 5, "a game to start")
    for _ in range(60):              # hard drop until the board is full
        if "game: over" in game.output():
            break
        game.send("H", gap=0.35)
    game.wait_for("game: over", 5, "game over")
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
    # Follow mode: the hand is over the left edge and closes (a drop). The
    # block is still in the middle; it must land at the left wall.
    game.send("X020Y500H", gap=0.4)
    # Then the hand moves to the right edge and drops again
    for _ in range(4):
        game.send("X980Y500", gap=0.1)
    game.send("X980Y500H", gap=0.3)
    # The hand leaves the picture: the game pauses by itself
    game.wait_for("game: paused", 4, "the game to pause when the hand has gone")
    ok("the game pauses when the hand leaves the camera's view")
    game.send("D", "H", gap=0.4)     # pause menu: save & quit (a drop selects)
    game.wait_exit(10, "save & quit")
    save = load_json(os.path.join(workdir, "game_state.json"), "game_state.json")
    if save.get("level") != 5 or save.get("lines_per_level") != 5 or "game_grid" not in save:
        fail(f"unexpected save file: {save}")
    grid = save["game_grid"]
    filled = sum(1 for row in grid for cell in row if cell)
    if filled != 8:
        fail(f"save file has {filled} filled cells after two hard drops, expected 8")
    columns = sorted({c for row in grid for c, cell in enumerate(row) if cell})
    if columns[0] != 0 or columns[-1] != 9 or any(3 < c < 6 for c in columns):
        fail(f"the two drops by hand should be at the walls, but filled columns are {columns}")
    ok(f"drops by hand land under the hand (columns {columns}); zen save & quit wrote "
       f"game_state.json at level 5")

    # ---- run 2: hand control was left on, so it comes back with the game
    profile = load_json(profile_path, "profile.json")
    profile["hand_control"] = True
    with open(profile_path, "w") as f:
        json.dump(profile, f)
    game = Game(exe, workdir, "run2")
    game.wait_for("listening on udp://127.0.0.1:5005", 20, "the game to restart")
    game.wait_for("hand: connected", 15, "the camera script to start with the game")
    ok("hand control saved as on starts the camera script with the game")
    time.sleep(1.0)
    game.send("C", gap=0.6)          # skip boot
    game.send("C", gap=0.6)          # zen setup
    game.send("D", "C", gap=0.4)     # "continue saved"
    game.wait_for("game: save loaded", 5, "the saved game to load")
    ok("continue saved loads the game")
    time.sleep(2.0)
    game.send("C", gap=0.6)          # pause
    game.send("U", "C", gap=0.4)     # pause menu: up wraps to "quit"
    game.wait_exit(10, "quit")
    ok("quit from the pause menu exits cleanly")
    game.wait_for_sidecar(["started", "answered", "quit"], 5,
                          "the camera script to be told to close when the game quits")
    ok("quitting the game closes the camera script")

    print("SMOKE TEST PASSED")


if __name__ == "__main__":
    main()
