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
./build/Tetris --test                  # C++ tests; non-zero exit code if any check fails
xvfb-run -a python3 tests/smoke_test.py build/Tetris   # end-to-end: plays the game via UDP
./build/Tetris --gesture-bind 0.0.0.0  # accept gestures from another machine (Windows → WSL)
python3 -m unittest gesture/test_gestures.py gesture/test_hand_control.py
```

Linux needs `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.
Headless screenshots: `xvfb-run -a -s "-screen 0 1024x768x24" <script>` with
`LIBGL_ALWAYS_SOFTWARE=1`; take them with ImageMagick `import -window root` and drive the game by
sending UDP commands (see below) instead of fake key presses.

## Layout

| Path | What |
|---|---|
| `src/game.*` | `Game` base class: piece movement, collision, `apply(Action)`, `hard_drop()`, ghost piece, `step_towards_column()`, wall kicks, `tick(now)` gravity with optional `lock_delay`, `piece_id` |
| `src/zen_mode.*` | `ZenSettings` (start level, lines per level), `level`, `drop_interval_for()`, `multiplier_for()`, `update_level()`, save/load incl. level data. Default `ZenMode()` = level 1, x1, 5 lines per level (the original tests rely on it) |
| `src/HighScores.*`, `src/Profile.*` | `scores.json` (top 5 + last run per mode) and `profile.json` (Zen setup choices); both tolerate missing/broken files |
| `src/Sound.*` | Beeps generated in code (`sound::play(Sfx::...)`); no-op without an audio device |
| `src/zen_mode.*`, `time_attack_mode.*`, `first_forty_mode.*`, `time_dependent_mode.*` | The three modes (scoring, finish rules, Zen save/load to `game_state.json`); `TimeDependentMode` has `pause_timer()`/`resume_timer()` |
| `src/board.*` | 20x10 grid (raw `int**`, not copyable), row clearing, drawing; records `last_cleared_rows`/`clear_events` for the flash effect |
| `src/Tetromino/` | Base `Tetromino` + one class per piece; each constructor sets its fixed `color_id` (I=1 … Z=7) |
| `src/Color.*` | Palette; `cell_color(id)` wraps ids > 7 (old saves used random ids up to 10) |
| `src/App.*` | The one window and the scenes (Boot, MainMenu, ZenSetup, Playing with countdown, Paused, GameOver), sidebar, effects (sliding block, sparks, trails, shake), sounds and high-score recording. It draws to an 800x700 render texture that is letterboxed into the resizable window; mouse coordinates are remapped with `SetMouseOffset`/`SetMouseScale` |
| `src/ui/Draw.*` | Phosphor themes (`set_phosphor`, `theme()`, `block_color`), `draw_cell`/`draw_ghost_cell`/`draw_empty_cell`, block-letter logo, font and text helpers, `mouse_active()` |
| `src/ui/Widgets.*` | `ui::text_button` and `ui::Menu` (terminal-style list with optional values; keyboard, gesture and mouse) |
| `assets/shaders/crt.fs` | CRT post-process shader applied to the 800x700 render texture (`time`, `strength` uniforms) |
| `src/input/` | `Action` enum, `InputManager` (keyboard with DAS 170 ms / ARR 50 ms + gestures), `GestureSource` (non-blocking UDP on 127.0.0.1:5005) |
| `gesture/gestures.py` | Detector with no camera dependency: landmarks → commands. Modes `flick` (default), `point` (index finger angle around the knuckle, learned resting angle, re-arm at rest), `palm` (old swipes, alias `swipe`), `position`. Pinch/fist/drop in all modes; tolerates `max_gap_frames` missing frames. All thresholds are in `Config` |
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
- Every feature needs tests. C++ tests live in `src/tests/` (`TestGame`, `TestModes`, `TestData`,
  `TestInput`) and use `test::check(ok, "name")` from `TestRunner.h`; `--test` fails if any check
  fails. Keep logic testable without a window (e.g. `ui::Menu::handle`, `AutoRepeat`, file paths as
  parameters) and never let tests touch the player's real files (use `test::temp_file`).
  Python: `gesture/test_*.py` (fakes for cv2/mediapipe; `pointing()`/`tilt()`/`hold()` build
  synthetic finger poses). When planting bugs in Python, clear `gesture/__pycache__` or set
  `PYTHONDONTWRITEBYTECODE=1`: a same-size edit within the same second can run stale bytecode. New screens/flows: extend
  `tests/smoke_test.py`. When a test passes first time, try breaking the code to make sure it
  would catch the bug.
- Look: lowercase terminal text, colours only from `ui::theme()`; test screens headlessly and
  check them with the CRT effect on.
- The MediaPipe model (`gesture/models/`) is downloaded at runtime and git-ignored; don't commit it.

## Git

- Work on logically named branches (`fix/wsl-camera`, `feature/crt-ui-zen-levels`, …) cut from
  the latest `main`, and open PRs into `main` of `Hussain5001/tetrisPlusPlus`.
- Keep this file up to date when the architecture, commands or conventions change.
