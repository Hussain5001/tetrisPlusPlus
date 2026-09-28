# Tetris++ — hands-free edition

An extended version of **TetrisX**, our university C++ Tetris clone
([Hussain5001/tetris](https://github.com/Hussain5001/tetris)). The original
repository is left untouched; this one adds:

- **A redesigned UI.** Everything runs in one resizable window, with bevelled blocks, a ghost piece,
  a line-clear flash, stat panels for each mode, and pause and game-over screens.
- **Hands-free play.** A small Python program watches your webcam, recognises hand
  gestures with MediaPipe and sends them to the game.

| Menu | Playing | Paused | Game over |
|---|---|---|---|
| ![menu](docs/screenshots/menu.png) | ![gameplay](docs/screenshots/gameplay.png) | ![pause](docs/screenshots/pause.png) | ![game over](docs/screenshots/gameover.png) |

## Building

```bash
git clone --recursive https://github.com/Hussain5001/tetrisPlusPlus.git
cd tetrisPlusPlus
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/Tetris            # Windows: build\Debug\Tetris.exe
```

On Linux you need the X11/OpenGL headers for raylib
(`sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`).

If you cloned without `--recursive`, run `git submodule update --init` first.

## Keyboard controls

| Key | Action |
|---|---|
| ← / → (hold to repeat) | move |
| ↑ or X | rotate |
| ↓ | soft drop |
| Space | hard drop |
| Esc or P | pause |
| Enter | select in menus |

## Hands-free play (camera gestures)

```bash
pip install -r gesture/requirements.txt      # mediapipe + opencv (Python 3.9-3.12)
./build/Tetris --gestures                    # starts the game and the camera script
# or run them separately:
./build/Tetris
python3 gesture/hand_control.py              # add --mode position to try position mode
```

The first run downloads the MediaPipe hand model (about 8 MB) to `gesture/models/`.
The "HAND CONTROL" box in the game turns green when the camera script is connected,
and every recognised gesture is shown briefly over the board.

| Gesture | In game | In menus |
|---|---|---|
| Swipe left / right | move one column | move the selection |
| Swipe down | hard drop | select |
| Swipe up **or** pinch (thumb and index finger) | rotate | move the selection up |
| Hold a fist for about half a second | pause | select |

**Position mode** (`--mode position`, or press `m` in the camera window): the falling
piece follows your hand sideways. It's faster than swiping for quick play; swipe down still drops.

Tips for responsive play:

- Swipes fire as soon as your hand speeds up, not when the movement ends. A short, quick
  flick works better than a long sweep.
- Bringing your hand back after a swipe is ignored, so you don't need to "reset" slowly.
- Speeds are measured relative to the size of your hand, so it works at any distance.
- Tuning: `--sensitivity 1.3` makes smaller swipes count, `--sensitivity 0.8` needs bigger
  ones, and `--no-vertical` turns off up/down swipes. Thresholds are in `gesture/gestures.py` (`Config`).
- Good lighting and a plain background help MediaPipe the most.

### Playing from WSL

WSL2 can't access your laptop's webcam (`Could not open camera/video: 0`). The game can stay
in WSL, but the hand tracking has to run on Windows. It sends gestures to the game over the network.

**Automatic:** `./build/Tetris --gestures` detects WSL and starts `gesture\run_windows.bat`
on Windows for you. You need Python 3.10–3.12 installed on Windows
([python.org](https://www.python.org/downloads/)). The first start sets up a Python environment
in `%LOCALAPPDATA%\tetrisplusplus` and installs MediaPipe, which takes a minute.

**Manual**, if you prefer to start it yourself:

- *Windows 11 with mirrored networking* (Windows and WSL share `localhost`). Add this to
  `%UserProfile%\.wslconfig`, run `wsl --shutdown`, then reopen WSL:
  ```ini
  [wsl2]
  networkingMode=mirrored
  ```
  Start `./build/Tetris` in WSL, then double-click `gesture\run_windows.bat` on Windows. From
  WSL, `explorer.exe gesture` opens that folder.
- *Default (NAT) networking*: start the game with `./build/Tetris --gesture-bind 0.0.0.0`, get
  the WSL address with `hostname -I` (first address), then on Windows run
  `gesture\run_windows.bat --host <that address>`.

The camera preview window opens on Windows. The game's "HAND CONTROL" box turns green once gestures arrive.

### How it works

```
webcam ──► hand_control.py ──(UDP 127.0.0.1:5005, e.g. "L", "H", "@4")──► Tetris
           MediaPipe landmarks                                            InputManager
           → gestures.py                                                  → Game::apply(Action)
```

The game reads the UDP socket without blocking every frame, so a gesture takes effect on
the next frame after it arrives. You can also drive the game from any other program or
script that sends these commands:

```bash
echo -n R | nc -u -w0 127.0.0.1 5005     # move right
```

## Tests

```bash
./build/Tetris --test                          # C++ unit tests (board, tetrominoes, modes)
python3 -m unittest gesture/test_gestures.py   # gesture detector with synthetic hands
```

## Credits

Original TetrisX team: Hussain Shakir, Manav Bijlani, Riddhi Agarwal and aryapranjan.
Built with [raylib](https://www.raylib.com/), [nlohmann/json](https://github.com/nlohmann/json)
and [MediaPipe](https://developers.google.com/mediapipe). Font: *monogram* by datagoblin (CC0).
See [User_Documentation.md](User_Documentation.md) for the game modes and scoring.
