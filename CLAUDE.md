# CLAUDE.md

Guidance for Claude (and humans) working on **Tetris++**, an extension of the
university project [Hussain5001/tetris](https://github.com/Hussain5001/tetris).
The original repo is frozen: never push to it. (Add it as the `upstream` remote if you
need to compare against it.)

**Tetris++ is a Windows game.** Hand control, packaging and the docs target Windows only. The
code is kept compiling on Linux for one reason: CI has a headless display there for the
end-to-end smoke test. Don't add Linux or WSL features or user instructions.

## Build, run, test

```powershell
git submodule update --init                # raylib + nlohmann/json live in vendor/
.\build.bat                                # configure + build Release, then start the game with hand control
cmake --build build --config Release       # rebuild only
build\Release\Tetris.exe                   # play; H switches hand control (the camera script) on and off
build\Release\Tetris.exe --test            # C++ tests; non-zero exit code if any check fails
py -m unittest discover -s gesture         # Python tests (set PYTHONDONTWRITEBYTECODE=1)
py tests/smoke_test.py build/Release/Tetris.exe   # end-to-end: plays the real game via UDP
```

The smoke test opens the real game window, plays sounds and needs UDP port 5005 free (close the
game first). It takes about a minute.

CI only (Linux): needs `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
libgl1-mesa-dev xvfb`; the smoke test runs as
`xvfb-run -a -s "-screen 0 1024x768x24" python tests/smoke_test.py build/Tetris`. Headless
screenshots are taken the same way with `LIBGL_ALWAYS_SOFTWARE=1` and ImageMagick
`import -window root`, driving the game with UDP commands (see below) instead of fake key presses.

## Layout

| Path | What |
|---|---|
| `src/game.*` | `Game` base class: piece movement, collision, `apply(Action)`, `hard_drop()`, ghost piece, `step_towards_column()` / `move_to_column()`, wall kicks, `tick(now)` gravity with optional `lock_delay`, `piece_id`, `next_block` (the preview; `random_block()` hands it out and takes a new one from the 7-piece bag). `block_attach()` is shared by all modes: lock the block, clear full rows, call the mode's `on_lines_cleared(n)` for scoring, spawn the next block, and end the game (`on_top_out()`) if it has no room |
| `src/zen_mode.*` | `ZenSettings` (start level, lines per level), `level`, `drop_interval_for()`, `multiplier_for()`, `update_level()`, save/load incl. level data (`load_game_state` returns false if there is no usable save). Default `ZenMode()` = level 1, x1, 5 lines per level (the original tests rely on it) |
| `src/time_attack_mode.*`, `first_forty_mode.*`, `time_dependent_mode.*` | The two timed modes (scoring, finish rules); `TimeDependentMode` has `pause_timer()`/`resume_timer()` on a steady clock and stops it when the board fills up |
| `src/AppRules.h` | Small decisions the App makes, as pure code without raylib so tests can check them: `rules::pause_on_focus_loss`, `rules::HandSwitch` (when the camera script is started and stopped: toggle, still starting, started by hand, closed by itself), `rules::HandFollow` (arrow keys win until the hand moves to another column), `rules::HandLostPause` (pause 1.5 s after the hand leaves, unless the keyboard takes over), `rules::background_command` |
| `src/HighScores.*`, `src/Profile.*` | `scores.json` (top 5 + last run per mode) and `profile.json` (Zen setup choices); both tolerate missing/broken files |
| `src/Sound.*` | Beeps generated in code (`sound::play(Sfx::...)`); no-op without an audio device |
| `src/board.*` | 20x10 grid (raw `int**`, not copyable), row clearing, drawing; records `last_cleared_rows`/`clear_events` for the flash effect. Note: `is_cell_within_bounds()` returns true for a cell *outside* the grid |
| `src/Tetromino/` | Base `Tetromino` + one class per piece; each constructor sets its fixed `color_id` (I=1 … Z=7) |
| `src/Color.*` | Palette; `cell_color(id)` wraps ids > 7 (old saves used random ids up to 10) |
| `src/App.*` | The one window and the scenes (Boot, MainMenu, ZenSetup, Playing with countdown, Paused, GameOver), sidebar, effects (sliding block, sparks, trails, shake), sounds and high-score recording. It draws to an 800x700 render texture that is letterboxed into the resizable window; mouse coordinates are remapped with `SetMouseOffset`/`SetMouseScale`. Hand control: `toggle_hand_control()` (H key, main-menu line), `update_hand_control()` every frame, `start_sidecar()` launches `gesture\run_windows.bat --exit-with-game` (Windows only) |
| `src/ui/Draw.*` | Phosphor themes (`set_phosphor`, `theme()`, `block_color`), `draw_cell`/`draw_ghost_cell`/`draw_empty_cell`, block-letter logo, font and text helpers, `mouse_active()` |
| `src/ui/Widgets.*` | `ui::text_button` and `ui::Menu` (terminal-style list with optional values; keyboard, gesture and mouse) |
| `assets/shaders/crt.fs` | CRT post-process shader applied to the 800x700 render texture (`time`, `strength` uniforms) |
| `src/input/` | `Action` enum, `InputManager` (keyboard with DAS 170 ms / ARR 50 ms + gestures), `GestureSource` (non-blocking UDP on 127.0.0.1:5005; answers the camera script's keep-alives, `has_sidecar()`, `quit_sidecar()`, the follow-mode pointer `pointer_x()`/`pointer_live()`), `ColumnTracker` (pointer → column with hysteresis), `UdpProbe` (a plain socket for the tests) |
| `src/tests/` | C++ tests, run by `Tetris --test`: `TestGame`, `TestModes`, `TestData`, `TestInput`, and `TestLegacy` (the original project's unit tests) |
| `tests/smoke_test.py`, `tests/fake_sidecar.py` | End-to-end test: starts the game, plays it through UDP commands and checks the saved files. Hand control is tested with a stand-in camera script (no camera) that the game starts through `TETRIS_SIDECAR_CMD` |
| `gesture/gestures.py` | Detector with no camera dependency: landmarks → commands. **`follow` is the default mode** (`_follow`): the palm centre inside a zone (`zone_width` hand sizes, centred with `recentre()`) becomes the pointer `X<n>Y<n>` through a `OneEuro` filter; a pinch (`pinch_gap`, only with middle and ring fingers straight) sends `U` on contact; a fist (mean `finger_curl` ≥ `fist_on` for `fist_frames` frames) sends `H` once and re-arms below `fist_off`, and that message carries the pointer from before the hand began to close. Finger poses come from MediaPipe's world landmarks when given. Older modes, to be removed: `flick` (fingertip swipes in hand sizes, `_strokes`/`_flick`; the return stroke is ignored until the tip is back where the swipe began), `point` (index finger angle around the knuckle, learned resting angle, re-arm at rest), `palm` (old swipes, alias `swipe`), `position`. Finger modes: drop = fingertip down fast (`_drop`; a bent finger only drops once it points again within `tap_time`), open palm held = `P` pause, fist does nothing. Palm/position: fist = `C`. Pinch rotates everywhere; tolerates `max_gap_frames` missing frames. All thresholds are in `Config` |
| `gesture/hand_control.py` | Webcam + MediaPipe Tasks `HandLandmarker` (VIDEO mode) → UDP. `LatestFrameReader` reads the camera on a thread and hands out only the newest frame (stale ones are skipped); video files go through `SequentialReader` and are timed by their own frame rate. `--stats` (`Stats`), `--record` (`Recorder`: JSONL with image + world landmarks and the commands sent), `--backend`. The preview is redrawn every `PREVIEW_INTERVAL`, not every frame |
| `gesture/replay.py` | Runs the detector over a `--record` file with other settings (`--set name=value`, `--mode`) and counts missed/extra/false commands against `--expect` |
| `gesture/test_follow.py`, `gesture/test_gestures.py`, `gesture/test_hand_control.py`, `gesture/test_replay.py` | Tests using synthetic hand poses and movements; OpenCV and MediaPipe are faked (`run_main` runs the real main loop on scripted frames). `test_follow.shape()`/`hand()` build a hand whose fingers bend at their joints (`grip`, `pinched`); `test_gestures.py` covers the older modes |
| `gesture/run_windows.bat` | Launcher for the camera script. Keeps a Python environment in `%LOCALAPPDATA%\tetrisplusplus` and rebuilds it whenever `gesture/requirements.txt` (pinned versions) changes |
| `build.bat`, `packaging/windows/` | Build from source and start; README.txt goes into the downloadable zip next to `Tetris.exe` |
| `.github/workflows/build.yml` | CI: Windows build + tests + `TetrisPlusPlus-windows` artifact (a release on `v*` tags); Linux build + tests + smoke test |
| `docs/screenshots/`, `Project_Plan/` | README screenshots; the original university project plan (historical, not maintained) |

## Gesture protocol (UDP → `GestureSource::poll`)

Single ASCII letters, several per datagram allowed:
`L` left, `R` right, `D` soft drop, `H` hard drop, `U` rotate, `P` pause, `C` confirm
(pause in game, select in menus), `B` back, `K` keep-alive, `@N` position mode target column,
`X<n>` `Y<n>` follow mode pointer (0–999 from the left / top edge of the hand's zone; sent before
any command in the same datagram, so a drop lands where the pointer says).
Any new command needs to be added in three places: `GestureSource.cpp`, `gestures.py`, and the README table.

The game answers the sender of keep-alives (only the camera script sends them, every 0.5 s):
`k` = the game is here, `Q` = close. The script closes on `Q` (hand control switched off, or the
game quit) and, when started with `--exit-with-game`, after 5 s without a `k`. On Windows a UDP
socket reports an error on its next read when nobody listens on the other side; both ends
treat that as "not there", not as a failure.

## Conventions

- Match the surrounding style: snake_case functions and members; comment each function the way the
  original code does; the original classes use 2- or 4-space indentation depending on the file.
- Keep game rules in the `Game`/mode classes and all drawing/layout in `App`/`ui`. Input always goes
  through `Action`, never through raw key checks in game logic.
- `GestureSource.cpp` must not include `raylib.h`, because it clashes with `winsock2.h` on Windows.
- Don't copy `Game`/`Board` objects (they own raw memory); `App` holds the game in a `unique_ptr`.
- Console output: game logic prints nothing. `App` prints one line per game event
  (`game: started <mode>`, `game: over`, `game: save loaded`), and `tests/smoke_test.py` waits for
  exactly these lines, so change both together.
- Every feature needs tests. C++ tests live in `src/tests/` and use `test::check(ok, "name")` from
  `TestRunner.h`; `--test` fails if any check fails. Keep logic testable without a window (e.g.
  `ui::Menu::handle`, `AutoRepeat`, file paths as parameters) and never let tests touch the
  player's real files (use `test::temp_file`).
  Python: `gesture/test_*.py` (fakes for cv2/mediapipe; `pointing()`/`tilt()`/`hold()` build
  synthetic finger poses). When planting bugs in Python, clear `gesture/__pycache__` or set
  `PYTHONDONTWRITEBYTECODE=1`: a same-size edit within the same second can run stale bytecode. New screens/flows: extend
  `tests/smoke_test.py`. When a test passes first time, try breaking the code to make sure it
  would catch the bug.
- Look: lowercase terminal text, colours only from `ui::theme()`; test screens headlessly and
  check them with the CRT effect on.
- The MediaPipe model (`gesture/models/`) is downloaded at runtime and git-ignored; don't commit it.
- Time camera frames with `time.perf_counter()`, never `time.monotonic()`: on Windows (Python
  < 3.13) that clock only ticks every 15.6 ms, half a frame, which spoils every speed the
  detector measures.
- Never start the real camera script from a test: set `TETRIS_SIDECAR_CMD` to a stand-in, as the
  smoke test does (the game then runs that command instead of `run_windows.bat`).
- Don't open the player's webcam from tests or tooling. The real script can be exercised without
  it on a video file (`--camera clip.avi --no-preview --stats`).
- Player data (`scores.json`, `profile.json`, `game_state.json`) is written to the folder the game
  is started from and is git-ignored.

## Git

- Work on logically named branches (`chore/windows-only-cleanup`, `feature/follow-gestures`, …)
  cut from the latest `main`, and open PRs into `main` of `Hussain5001/tetrisPlusPlus`.
- Keep this file up to date when the architecture, commands or conventions change.
