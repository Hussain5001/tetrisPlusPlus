# CLAUDE.md

Guidance for Claude (and humans) working on **Tetris++**, an extension of the
university project [Hussain5001/tetris](https://github.com/Hussain5001/tetris).
The original repo is frozen; it's available here as the `upstream` remote.
Never push to it.

## Build, run, test

```bash
git submodule update --init            # raylib + nlohmann/json live in vendor/
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/Tetris                         # play
./build/Tetris --gestures              # also launch gesture/hand_control.py
./build/Tetris --test                  # C++ unit tests (all lines should say "passed")
./build/Tetris --gesture-bind 0.0.0.0  # accept gestures from another machine (Windows → WSL)
python3 -m unittest gesture/test_gestures.py
```

Linux needs `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.
Headless screenshots: `xvfb-run -a -s "-screen 0 1024x768x24" <script>` with
`LIBGL_ALWAYS_SOFTWARE=1`; take them with ImageMagick `import -window root` and drive the game by
sending UDP commands (see below) instead of fake key presses.

## Layout

| Path | What |
|---|---|
| `src/game.*` | `Game` base class: piece movement, collision, `apply(Action)`, `hard_drop()`, ghost piece, `step_towards_column()` |
| `src/zen_mode.*`, `time_attack_mode.*`, `first_forty_mode.*`, `time_dependent_mode.*` | The three modes (scoring, finish rules, Zen save/load to `game_state.json`); `TimeDependentMode` has `pause_timer()`/`resume_timer()` |
| `src/board.*` | 20x10 grid (raw `int**`, not copyable), row clearing, drawing; records `last_cleared_rows`/`clear_events` for the flash effect |
| `src/Tetromino/` | Base `Tetromino` + one class per piece; each constructor sets its fixed `color_id` (I=1 … Z=7) |
| `src/Color.*` | Palette; `cell_color(id)` wraps ids > 7 (old saves used random ids up to 10) |
| `src/App.*` | The one window and the scenes (MainMenu, ZenChoice, Playing, Paused, GameOver), HUD and overlays. It draws to an 800x700 render texture that is letterboxed into the resizable window; mouse coordinates are remapped with `SetMouseOffset`/`SetMouseScale` |
| `src/ui/Draw.*` | Theme colours, bevelled `draw_cell`, `draw_ghost_cell`, the shared font (`assets/monogram.ttf`) and text helpers |
| `src/ui/Widgets.*` | `ui::button` (immediate mode) and `ui::Menu` (keyboard, gesture and mouse navigation) |
| `src/input/` | `Action` enum, `InputManager` (keyboard with DAS 170 ms / ARR 50 ms + gestures), `GestureSource` (non-blocking UDP on 127.0.0.1:5005) |
| `gesture/gestures.py` | Detector with no camera dependency: landmarks → commands. All thresholds are in `Config` |
| `gesture/hand_control.py` | Webcam + MediaPipe Tasks `HandLandmarker` (VIDEO mode) → UDP |
| `gesture/test_gestures.py` | Tests using synthetic hand poses and movements |
| `build.bat`, `packaging/windows/` | Windows: build from source and start; `play.bat` + README.txt go into the downloadable zip |
| `.github/workflows/build.yml` | CI: Linux build + C++ and Python tests; Windows build + tests + `TetrisPlusPlus-windows` artifact (a release on `v*` tags) |
| `gesture/run_windows.bat` | Windows launcher for the camera script (venv in `%LOCALAPPDATA%`); used when the game runs in WSL, which has no webcam. `App::start_sidecar()` starts it through WSL interop |

## Gesture protocol (UDP → `GestureSource::poll`)

Single ASCII letters, several per datagram allowed:
`L` left, `R` right, `D` soft drop, `H` hard drop, `U` rotate, `P` pause, `C` confirm
(pause in game, select in menus), `B` back, `K` keep-alive, `@N` position mode target column.
Any new command needs to be added in three places: `GestureSource.cpp`, `gestures.py`, and the README table.

## Conventions

- Match the surrounding style: snake_case functions and members; comment each function the way the
  original code does; the original classes use 2- or 4-space indentation depending on the file.
- Keep game rules in the `Game`/mode classes and all drawing/layout in `App`/`ui`. Input always goes
  through `Action`, never through raw key checks in game logic.
- `GestureSource.cpp` must not include `raylib.h`, because it clashes with `winsock2.h` on Windows.
- Don't copy `Game`/`Board` objects (they own raw memory); `App` holds the game in a `unique_ptr`.
- Keep `./build/Tetris --test` and the Python tests passing, and extend them when changing logic.
- The MediaPipe model (`gesture/models/`) is downloaded at runtime and git-ignored; don't commit it.

## Git

- Work on logically named branches (`fix/wsl-camera`, `feature/crt-ui-zen-levels`, …) cut from
  the latest `main`, and open PRs into `main` of `Hussain5001/tetrisPlusPlus`.
- Keep this file up to date when the architecture, commands or conventions change.
