#!/usr/bin/env python3
"""Stands in for gesture/hand_control.py in the smoke test.

No camera and no hand tracking: only the link with the game. It sends a
keep-alive ("K") ten times a second like the real camera script, writes down
what the game does in a log file, and closes when the game tells it to ("Q").

    fake_sidecar.py <port> <log file>

The smoke test makes the game start this instead of the real script by
setting TETRIS_SIDECAR_CMD.
"""
import socket
import sys
import time


def main():
    port, log_path = int(sys.argv[1]), sys.argv[2]

    def log(word):
        with open(log_path, "a") as f:
            f.write(word + "\n")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(0.1)
    log("started")
    answered = False
    give_up = time.time() + 60          # never stay behind if a test goes wrong
    while time.time() < give_up:
        sock.sendto(b"K", ("127.0.0.1", port))
        try:
            data = sock.recv(64)
        except socket.timeout:
            continue
        except OSError:                 # the game isn't listening (Windows says so)
            time.sleep(0.1)
            continue
        if b"k" in data and not answered:
            answered = True
            log("answered")
        if b"Q" in data:
            log("quit")
            return
        time.sleep(0.1)
    log("gave up")


if __name__ == "__main__":
    main()
