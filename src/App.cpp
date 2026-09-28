#include "App.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Color.h"
#include "Sound.h"
#include "first_forty_mode.h"
#include "rlgl.h"
#include "time_attack_mode.h"
#include "time_dependent_mode.h"
#include "ui/Draw.h"
#include "zen_mode.h"

namespace {
const char* kModeNames[] = {"", "zen", "time attack", "first 40 lines"};
const char* kModeHints[] = {
    "", "play at your own pace. pick a level, save and come back later.",
    "score as much as you can in 120 seconds.",
    "clear 40 lines as fast as you can."};

struct Pace {
  const char* name;
  int lines;
};
const Pace kPaces[] = {{"chill", 10}, {"steady", 8}, {"rising", 5}, {"brutal", 3}};

const double kTimeAttackLength = 120.0;
const int kFortyLineGoal = 40;
const float kCountdown = 1.6f;

// True when running under Windows Subsystem for Linux
bool running_in_wsl() {
  std::ifstream version("/proc/version");
  std::string text;
  std::getline(version, text);
  for (char& c : text) c = (char)std::tolower((unsigned char)c);
  return text.find("microsoft") != std::string::npos;
}

std::string format_time(double seconds) {
  if (seconds < 0) seconds = 0;
  char buf[32];
  int minutes = (int)seconds / 60;
  std::snprintf(buf, sizeof(buf), "%d:%04.1f", minutes, seconds - minutes * 60);
  return buf;
}

// 12400 -> "12,400"
std::string format_int(double value) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.0f", value);
  std::string digits = buf;
  std::string out;
  int start = digits[0] == '-' ? 1 : 0;
  for (size_t i = 0; i < digits.size(); i++) {
    out += digits[i];
    size_t left = digits.size() - i - 1;
    if ((int)i >= start && left > 0 && left % 3 == 0) out += ',';
  }
  return out;
}

float frand(float lo, float hi) {
  return lo + (hi - lo) * (float)GetRandomValue(0, 10000) / 10000.0f;
}
}  // namespace

// ------------------------------------------------------------------ setup

void App::run_menu() {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(kWidth, kHeight, "Tetris++");
  SetWindowMinSize(kWidth / 2, kHeight / 2);
  SetExitKey(KEY_NULL);  // Esc pauses instead of closing the window
  SetTargetFPS(60);
  ui::load_font();
  sound::init();

  profile.load();
  scores.load();
  apply_profile();

  RenderTexture2D target = LoadRenderTexture(kWidth, kHeight);
  SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

  // CRT effect shader (falls back to a plain picture if it can't load)
  std::string shader_paths[] = {
      std::string(GetApplicationDirectory()) + "assets/shaders/crt.fs",
      "assets/shaders/crt.fs"};
  for (const std::string& path : shader_paths) {
    if (!FileExists(path.c_str())) continue;
    crt_shader = LoadShader(0, path.c_str());
    crt_loaded = crt_shader.id > 0 && crt_shader.id != rlGetShaderIdDefault();
    if (crt_loaded) {
      crt_time_loc = GetShaderLocation(crt_shader, "time");
      crt_strength_loc = GetShaderLocation(crt_shader, "strength");
    }
    break;
  }

  in_wsl = running_in_wsl();
  if (in_wsl && launch_gesture_sidecar) gesture_bind_any = true;
  input.reset(new InputManager(gesture_bind_any));
  if (launch_gesture_sidecar) start_sidecar();

  main_menu = ui::Menu({"zen", "time attack", "first 40 lines", "quit"});
  over_menu = ui::Menu({"play again", "main menu", "quit"});
  refresh_zen_menu();
  zen_menu.focus = 6;  // "start"

  // Boot screen text
  const char* phosphor_names[] = {"amber", "green", "multi"};
  boot_lines = {
      "tetris++ v2.0",
      "built by hussain5001",
      "",
      "memory check ............ 640k ok",
      std::string("phosphor ................ ") + phosphor_names[profile.phosphor],
      std::string("crt driver .............. ") +
          (crt_loaded ? "ok" : "unavailable"),
      std::string("sound ................... ") +
          (sound::available() ? "ok" : "no device"),
      std::string("gesture link ............ ") +
          (input->gestures().is_open() ? "listening :5005" : "port busy"),
      "high scores ............. " + std::to_string(scores.total_entries()) + " found",
      "",
      "press any key"};
  boot_start = GetTime();

  while (!WindowShouldClose() && !quit) {
    // Scale the virtual screen to the window, keeping its aspect ratio
    float scale = std::min((float)GetScreenWidth() / kWidth,
                           (float)GetScreenHeight() / kHeight);
    float offset_x = (GetScreenWidth() - kWidth * scale) / 2;
    float offset_y = (GetScreenHeight() - kHeight * scale) / 2;
    SetMouseOffset((int)-offset_x, (int)-offset_y);
    SetMouseScale(1 / scale, 1 / scale);

    ui::update_mouse();
    handle_global_keys();
    std::vector<Action> actions = input->poll();

    BeginTextureMode(target);
    ClearBackground(ui::theme().background);
    frame(actions);
    EndTextureMode();

    // Screen shake after a big line clear
    float shake = 0;
    if (GetTime() < shake_until) shake = (float)(shake_until - GetTime()) * 22;
    float sx = frand(-shake, shake), sy = frand(-shake, shake);

    BeginDrawing();
    ClearBackground(BLACK);
    bool use_crt = crt_loaded && profile.crt;
    if (use_crt) {
      float t = (float)GetTime(), strength = 1.0f;
      SetShaderValue(crt_shader, crt_time_loc, &t, SHADER_UNIFORM_FLOAT);
      SetShaderValue(crt_shader, crt_strength_loc, &strength, SHADER_UNIFORM_FLOAT);
      BeginShaderMode(crt_shader);
    }
    DrawTexturePro(target.texture, {0, 0, (float)kWidth, (float)-kHeight},
                   {offset_x + sx, offset_y + sy, kWidth * scale, kHeight * scale},
                   {0, 0}, 0, WHITE);
    if (use_crt) EndShaderMode();
    EndDrawing();
  }

  game.reset();
  input.reset();
  if (crt_loaded) UnloadShader(crt_shader);
  UnloadRenderTexture(target);
  sound::shutdown();
  ui::unload_font();
  CloseWindow();
}

// Applies the saved profile (colours, sound)
void App::apply_profile() {
  ui::set_phosphor(profile.phosphor);
  sound::set_enabled(profile.sound);
}

// Keys that work everywhere
void App::handle_global_keys() {
  if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
  if (IsKeyPressed(KEY_M)) {
    profile.sound = !profile.sound;
    apply_profile();
    profile.save();
    refresh_zen_menu();
  }
  // Pause automatically when the window loses focus
  bool focused = IsWindowFocused();
  if (was_focused && !focused && scene == Scene::Playing) pause_game();
  was_focused = focused;
}

// Updates and draws one frame of the current scene
void App::frame(const std::vector<Action>& actions) {
  switch (scene) {
    case Scene::Boot: boot_scene(actions); break;
    case Scene::MainMenu: main_menu_scene(actions); break;
    case Scene::ZenSetup: zen_setup_scene(actions); break;
    case Scene::Playing:
      update_game(actions);
      if (!game) break;
      draw_board();
      draw_sidebar();
      draw_effects();
      draw_countdown();
      draw_gesture_popup();
      break;
    case Scene::Paused: pause_scene(actions); break;
    case Scene::GameOver: game_over_scene(actions); break;
  }
}

// ------------------------------------------------------------------ scenes

void App::boot_scene(const std::vector<Action>& actions) {
  draw_rain(true);
  const ui::Theme& t = ui::theme();
  // Type the lines out, about 150 characters a second
  int budget = (int)((GetTime() - boot_start) * 150);
  float y = 120;
  bool done = true;
  for (const std::string& line : boot_lines) {
    int shown = std::min((int)line.size(), budget);
    budget -= (int)line.size() + 4;
    std::string part = line.substr(0, std::max(shown, 0));
    bool typing = shown >= 0 && shown < (int)line.size();
    if (shown < (int)line.size()) done = false;
    if (typing || (&line == &boot_lines.back() && done)) {
      if (ui::cursor_on()) part += "_";
    }
    Color c = &line == &boot_lines.front() ? t.bright : t.text;
    if (line.find("..") != std::string::npos) c = t.dim;
    ui::text(part.c_str(), 110, y, 30, c);
    y += 34;
    if (shown < 0) break;
  }
  bool skip = !actions.empty() || GetKeyPressed() != 0 ||
              IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  double typed_for = GetTime() - boot_start;
  if (skip || (done && typed_for > 3.2)) {
    scene = Scene::MainMenu;
    sound::play(Sfx::MenuSelect);
  }
}

void App::main_menu_scene(const std::vector<Action>& actions) {
  const ui::Theme& t = ui::theme();
  draw_rain(true);
  ui::draw_logo(kWidth / 2, 70, 16, (float)GetTime());
  ui::text_centered("hands-free edition", kWidth / 2, 162, 28, t.dim);

  int choice = main_menu.run(actions, 250, 240, 300, 46, 34);

  // Best and last result for the focused mode
  int focused_mode = main_menu.focus + 1;
  if (focused_mode <= 3) {
    ui::text_centered(kModeHints[focused_mode], kWidth / 2, 440, 24, t.dim);
    const ScoreEntry* best = scores.best(focused_mode);
    const ScoreEntry* last = scores.last(focused_mode);
    std::string b = best ? format_score(focused_mode, *best) + "  (" + best->date + ")"
                         : "---";
    std::string l = last ? format_score(focused_mode, *last) : "---";
    draw_score_line(250, 480, 300, "best", b, true);
    draw_score_line(250, 512, 300, "last", l, false);
  }

  ui::text("up/down move   enter select   m mute   f11 fullscreen", 40, 610, 22, t.dim);
  ui::text("v2.0 // built by hussain5001", 40, 650, 24, t.dim);
  draw_hand_status(kWidth - 40, 650, true);

  if (choice == 0) {
    refresh_zen_menu();
    zen_menu.focus = 6;
    scene = Scene::ZenSetup;
  } else if (choice == 1 || choice == 2) {
    start_game(choice + 1, false);
  } else if (choice == 3) {
    quit = true;
  }
}

// Rebuilds the value column of the Zen setup screen from the profile
void App::refresh_zen_menu() {
  int focus = zen_menu.focus;
  zen_menu = ui::Menu({"start level", "pace", "phosphor", "ghost piece", "crt effect",
                       "sound", "start", "continue saved", "back"});
  zen_menu.values = {std::to_string(profile.zen_level),
                     kPaces[profile.zen_pace].name,
                     ui::phosphor_name(profile.phosphor),
                     profile.ghost ? "on" : "off",
                     crt_loaded ? (profile.crt ? "on" : "off") : "n/a",
                     profile.sound ? "on" : "off",
                     "", "", ""};
  zen_menu.disabled = {false, false, false, false, false, false, false,
                       !FileExists("game_state.json"), false};
  zen_menu.focus = focus;
}

void App::zen_setup_scene(const std::vector<Action>& actions) {
  const ui::Theme& t = ui::theme();
  draw_rain(true);
  ui::text("zen // setup", 190, 50, 44, t.bright);
  ui::text("left/right change   up/down move   enter select", 190, 100, 22, t.dim);

  int choice = zen_menu.run(actions, 190, 150, 420, 42, 32);
  for (Action a : actions) {
    if (a == Action::Pause || a == Action::Back) choice = 8;
  }

  if (zen_menu.changed_row >= 0) {
    int d = zen_menu.change;
    switch (zen_menu.changed_row) {
      case 0: profile.zen_level = (profile.zen_level - 1 + d + 10) % 10 + 1; break;
      case 1: profile.zen_pace = (profile.zen_pace + d + 4) % 4; break;
      case 2: profile.phosphor = (profile.phosphor + d + 3) % 3; break;
      case 3: profile.ghost = !profile.ghost; break;
      case 4: profile.crt = !profile.crt; break;
      case 5: profile.sound = !profile.sound; break;
    }
    apply_profile();
    profile.save();
    refresh_zen_menu();
  }

  // What the chosen level and pace mean
  char info[128];
  std::snprintf(info, sizeof(info), "speed %.2fs/row   score x%.1f   level up every %d lines",
                ZenMode::drop_interval_for(profile.zen_level),
                ZenMode::multiplier_for(profile.zen_level), kPaces[profile.zen_pace].lines);
  ui::text(info, 190, 540, 20, t.dim);
  const ScoreEntry* best = scores.best(1);
  const ScoreEntry* last = scores.last(1);
  draw_score_line(190, 580, 420, "best", best ? format_score(1, *best) : "---", true);
  draw_score_line(190, 612, 420, "last", last ? format_score(1, *last) : "---", false);

  if (choice == 6) {
    start_game(1, false);
  } else if (choice == 7) {
    start_game(1, true);
  } else if (choice == 8) {
    scene = Scene::MainMenu;
  }
}

void App::pause_scene(const std::vector<Action>& actions) {
  draw_board();
  draw_sidebar();
  Rectangle panel = {kWidth / 2 - 200.f, 180, 400, 110.f + pause_menu.items.size() * 44};
  draw_panel(panel, "paused");
  ui::text_centered("swipe to choose, fist to select", kWidth / 2, panel.y + 58, 22,
                    ui::theme().dim);
  int choice = pause_menu.run(actions, panel.x + 60, panel.y + 96, 280, 44, 32);
  for (Action a : actions) {
    if (a == Action::Pause || a == Action::Back) choice = 0;
  }
  const std::string picked = choice >= 0 ? pause_menu.items[choice] : std::string();
  if (picked == "resume") {
    resume_game();
  } else if (picked == "save & quit") {
    static_cast<ZenMode*>(game.get())->save_game_state();
    quit = true;
  } else if (picked == "main menu") {
    game.reset();
    scene = Scene::MainMenu;
  } else if (picked == "quit") {
    quit = true;
  }
}

void App::game_over_scene(const std::vector<Action>& actions) {
  const ui::Theme& t = ui::theme();
  draw_board();
  draw_sidebar();
  draw_effects();

  Rectangle panel = {kWidth / 2 - 250.f, 70, 500, 560};
  std::string title = game_over_title();
  draw_panel(panel, title.c_str());

  float x = panel.x + 40, w = panel.width - 80, y = panel.y + 70;
  if (mode == 3 && !last_eligible) {
    draw_score_line(x, y, w, "lines", std::to_string(last_result.lines) + " / 40", true);
  } else {
    draw_score_line(x, y, w, mode == 3 ? "time" : "score", format_score(mode, last_result),
                    true);
  }
  y += 36;
  std::string rank_text;
  if (last_rank == 1) rank_text = "new best!";
  else if (last_rank > 1) rank_text = "rank #" + std::to_string(last_rank) + " of 5";
  else if (mode == 3 && !last_eligible) rank_text = "finish 40 lines to get on the board";
  else rank_text = "not in the top 5 this time";
  bool blink = last_rank == 1 && !ui::cursor_on();
  ui::text(("> " + rank_text).c_str(), x, y, 28,
           blink ? t.dim : (last_rank > 0 ? t.bright : t.dim));
  y += 50;

  ui::text("top 5", x, y, 26, t.dim);
  y += 32;
  const std::vector<ScoreEntry>& top = scores.top(mode);
  for (int i = 0; i < HighScores::kTableSize; i++) {
    std::string row = std::to_string(i + 1) + ".";
    std::string value = "---", detail;
    if (i < (int)top.size()) {
      value = format_score(mode, top[i]);
      detail = top[i].date;
      if (mode == 1 && top[i].level > 0) detail = "lv" + std::to_string(top[i].level) + "  " + detail;
    }
    Color c = (i + 1 == last_rank) ? t.bright : t.text;
    if (i + 1 == last_rank) DrawRectangle((int)x - 8, (int)y - 2, (int)w + 16, 30, ColorAlpha(t.text, 0.12f));
    ui::text(row.c_str(), x, y, 26, c);
    ui::text(value.c_str(), x + 40, y, 26, c);
    ui::text_right(detail.c_str(), x + w, y, 24, ColorAlpha(c, 0.7f));
    y += 32;
  }

  int choice = over_menu.run(actions, x + 60, panel.y + panel.height - 150, w - 120, 44, 32);
  if (choice == 0) {
    start_game(mode, false);
  } else if (choice == 1) {
    game.reset();
    scene = Scene::MainMenu;
  } else if (choice == 2) {
    quit = true;
  }
}

// ------------------------------------------------------------------ game flow

void App::start_game(int new_mode, bool load_saved) {
  mode = new_mode;
  game.reset();  // destroy the old game before building the new one
  if (mode == 1) {
    ZenMode* zen;
    if (load_saved) {
      zen = new ZenMode();
      zen->load_game_state();
    } else {
      ZenSettings settings;
      settings.start_level = profile.zen_level;
      settings.lines_per_level = kPaces[profile.zen_pace].lines;
      zen = new ZenMode(settings);
    }
    game.reset(zen);
    seen_level = zen->level;
  } else if (mode == 2) {
    game.reset(new TimeAttackMode());
  } else {
    game.reset(new FirstFortyMode());
  }
  game->lock_delay = 0.5;
  seen_clear_events = 0;
  seen_piece_id = game->piece_id;
  drawn_piece_id = -1;
  lines_this_game = 0;
  timer_started = false;
  particles.clear();
  float_texts.clear();
  trails.clear();
  flash_rows.clear();

  if (mode == 1) {
    pause_menu = ui::Menu({"resume", "save & quit", "main menu", "quit"});
  } else {
    pause_menu = ui::Menu({"resume", "main menu", "quit"});
  }
  over_menu.focus = 0;
  scene = Scene::Playing;
  begin_countdown();
}

void App::begin_countdown() {
  counting = true;
  countdown_end = GetTime() + kCountdown;
  last_countdown_beep = -1;
}

void App::update_game(const std::vector<Action>& actions) {
  if (counting) {
    for (Action a : actions) {
      if (a == Action::Pause) {
        pause_game();
        return;
      }
    }
    double left = countdown_end - GetTime();
    int beat = (int)std::ceil(left / (kCountdown / 3));
    if (beat != last_countdown_beep && beat > 0) {
      sound::play(Sfx::Countdown);
      last_countdown_beep = beat;
    }
    if (left > 0) return;
    counting = false;
    sound::play(Sfx::Go);
    if (auto timed = dynamic_cast<TimeDependentMode*>(game.get())) {
      if (!timer_started) timed->game_start();
      else timed->resume_timer();
    }
    timer_started = true;
    game->fall_start = GetTime();
  }

  double score_before = game->score;
  for (Action a : actions) {
    if (a == Action::Pause || a == Action::Confirm) {
      pause_game();
      return;
    }
    int col = game->current_block.get_col_offset();
    int rot = game->current_block.current_rotation;
    int id = game->piece_id;
    if (a == Action::HardDrop && !game->game_over) {
      // Remember where the block was, for the drop trail
      Trail trail;
      std::vector<Position> from = game->current_block.get_current_position();
      std::vector<Position> to = game->ghost_position();
      for (size_t i = 0; i < from.size(); i++) {
        trail.columns.push_back(from[i].column);
        trail.top_rows.push_back(from[i].row);
        trail.bottom_rows.push_back(to[i].row);
      }
      trail.start = (float)GetTime();
      trails.push_back(trail);
      sound::play(Sfx::HardDrop);
    }
    game->apply(a);
    if (game->piece_id == id) {
      if (game->current_block.current_rotation != rot) sound::play(Sfx::Rotate);
      else if (game->current_block.get_col_offset() != col) sound::play(Sfx::Move);
    }
  }

  // Position mode: the piece follows the hand, one column per frame
  int column = input->gestures().target_column();
  if (column >= 0) {
    game->step_towards_column(column);
  }
  game->fall_block();

  if (mode == 3) {
    game->score = game->get_score();  // First 40 Lines scores by time
  }

  // Events since last frame
  bool cleared = game->game_grid.clear_events != seen_clear_events;
  if (cleared) {
    seen_clear_events = game->game_grid.clear_events;
    std::vector<int> rows = game->game_grid.last_cleared_rows;
    lines_this_game += (int)rows.size();
    spawn_line_clear(rows, game->score - score_before);
    sound::play(rows.size() >= 4 ? Sfx::Tetris : Sfx::Clear);
  }
  if (game->piece_id != seen_piece_id) {
    if (!cleared) sound::play(Sfx::Lock);
    seen_piece_id = game->piece_id;
  }
  if (mode == 1) {
    auto zen = static_cast<ZenMode*>(game.get());
    if (zen->level != seen_level) {
      seen_level = zen->level;
      float_texts.push_back({"level " + std::to_string(zen->level),
                             {kBoardX + 150.f, kBoardY + 200.f}, (float)GetTime()});
      sound::play(Sfx::LevelUp);
    }
  }

  if (!game->game_over && game->is_game_finished()) {
    game->game_over = true;
  }
  if (game->game_over) end_game();
}

void App::pause_game() {
  if (!game) return;
  if (auto timed = dynamic_cast<TimeDependentMode*>(game.get())) {
    if (timer_started) timed->pause_timer();
  }
  pause_menu.focus = 0;
  scene = Scene::Paused;
  sound::play(Sfx::Pause);
}

void App::resume_game() {
  scene = Scene::Playing;
  begin_countdown();  // 3-2-1 before play continues
}

void App::end_game() {
  if (auto timed = dynamic_cast<TimeDependentMode*>(game.get())) {
    if (timed->timer_on) timed->game_end();
  }
  ScoreEntry entry;
  entry.lines = lines_this_game;
  if (mode == 1) {
    auto zen = static_cast<ZenMode*>(game.get());
    entry.value = zen->score;
    entry.lines = zen->total_lines_cleared;
    entry.level = zen->settings.start_level;
  } else if (mode == 2) {
    entry.value = game->score;
  } else {
    auto forty = static_cast<FirstFortyMode*>(game.get());
    entry.value = forty->elapsed_seconds();
    entry.lines = forty->total_lines_cleared;
  }
  last_eligible = mode != 3 || entry.lines >= kFortyLineGoal;
  last_rank = scores.record(mode, entry, last_eligible);
  last_result = *scores.last(mode);

  sound::play(last_rank == 1 ? Sfx::Tetris : Sfx::GameOver);
  over_menu.focus = 0;
  scene = Scene::GameOver;
}

std::string App::game_over_title() {
  if (mode == 2) {
    auto attack = static_cast<TimeAttackMode*>(game.get());
    if (attack->elapsed_seconds() >= kTimeAttackLength) return "time's up";
  }
  if (mode == 3 && last_eligible) return "complete";
  return "game over";
}

std::string App::format_score(int for_mode, const ScoreEntry& entry) const {
  if (for_mode == 3) {
    if (entry.lines < kFortyLineGoal) return std::to_string(entry.lines) + " lines";
    return format_time(entry.value);
  }
  return format_int(entry.value);
}

// ------------------------------------------------------------------ drawing

// Faint falling characters behind the menus
void App::draw_rain(bool animate) {
  const char* glyphs = "01[]<>!/\\=+-.#:";
  int glyph_count = 15;
  if (rain.empty()) {
    for (int x = 0; x < kWidth / 20; x++) {
      RainColumn col;
      col.y = frand(-kHeight, kHeight);
      col.speed = frand(18, 60);
      for (int i = 0; i < 12; i++) col.chars += glyphs[GetRandomValue(0, glyph_count - 1)];
      rain.push_back(col);
    }
  }
  float dt = animate ? GetFrameTime() : 0;
  const ui::Theme& t = ui::theme();
  for (size_t x = 0; x < rain.size(); x++) {
    RainColumn& col = rain[x];
    col.y += col.speed * dt;
    if (col.y > kHeight + 240) {
      col.y = -240;
      col.chars[GetRandomValue(0, 11)] = glyphs[GetRandomValue(0, glyph_count - 1)];
    }
    for (int i = 0; i < 12; i++) {
      char s[2] = {col.chars[i], 0};
      float alpha = 0.03f + 0.07f * i / 11.0f;  // brightest at the head
      ui::text(s, x * 20.0f + 4, col.y + i * 20, 20, ColorAlpha(t.text, alpha));
    }
  }
}

void App::draw_board() {
  const ui::Theme& t = ui::theme();
  int cell = game->game_grid.get_cell_size();
  int cols = game->game_grid.get_num_cols();
  int rows = game->game_grid.get_num_rows();
  int width = cols * cell;

  // Frame drawn with characters, like the 1984 original: <! ... !> and \/\/
  for (int r = 0; r < rows; r++) {
    float y = (float)kBoardY + r * cell;
    ui::text_right("<!", kBoardX - 2, y, 30, t.dim);
    ui::text("!>", kBoardX + width + 4, y, 30, t.dim);
  }
  float by = (float)kBoardY + rows * cell;
  ui::text_right("<!", kBoardX - 2, by, 30, t.dim);
  ui::text("!>", kBoardX + width + 4, by, 30, t.dim);
  for (int c = 0; c < cols; c++) {
    ui::text_centered("==", kBoardX + c * cell + cell / 2.0f, by, 30, t.dim);
    ui::text_centered("\\/", kBoardX + c * cell + cell / 2.0f, by + 26, 30, t.faint);
  }

  game->game_grid.draw(kBoardX, kBoardY);

  if (!game->game_over) {
    // Ghost piece
    if (profile.ghost) {
      Tetromino ghost = game->current_block;
      std::vector<Position> landing = game->ghost_position();
      ghost.move(landing[0].row - game->current_block.get_current_position()[0].row, 0);
      ghost.draw_ghost(kBoardX, kBoardY);
    }

    // The falling block slides smoothly between cells
    Vector2 target = {(float)game->current_block.get_col_offset() * cell,
                      (float)game->current_block.get_row_offset() * cell};
    if (game->piece_id != drawn_piece_id) {
      piece_pos = target;
      drawn_piece_id = game->piece_id;
    }
    float k = std::min(1.0f, GetFrameTime() * 30);
    piece_pos.x += (target.x - piece_pos.x) * k;
    piece_pos.y += (target.y - piece_pos.y) * k;
    game->current_block.draw(kBoardX + (int)std::lround(piece_pos.x - target.x),
                             kBoardY + (int)std::lround(piece_pos.y - target.y));
  }

  // Line clear: a bright band that sweeps out from the middle, then fades
  float ft = (float)(GetTime() - flash_start);
  if (ft < 0.35f) {
    float grow = std::min(1.0f, ft / 0.12f);
    float alpha = ft < 0.12f ? 0.9f : 0.9f * (1 - (ft - 0.12f) / 0.23f);
    for (int row : flash_rows) {
      float w = width * grow;
      DrawRectangle((int)(kBoardX + (width - w) / 2), kBoardY + row * cell, (int)w, cell,
                    ColorAlpha(t.bright, alpha));
    }
  }
}

void App::draw_score_line(float x, float y, float width, const char* label,
                          const std::string& value, bool bright) {
  const ui::Theme& t = ui::theme();
  ui::text((std::string("> ") + label).c_str(), x, y, 28, t.dim);
  ui::text_right(value.c_str(), x + width, y, 28, bright ? t.bright : t.text);
}

void App::draw_sidebar() {
  const ui::Theme& t = ui::theme();
  float x = kSideX, w = kSideW;
  ui::text(kModeNames[mode], x, 26, 36, t.bright);
  ui::text_right("tetris++", x + w, 34, 22, t.dim);
  DrawRectangle((int)x, 64, (int)w, 1, t.faint);

  float y = 80, lh = 36;
  if (mode == 1) {
    auto zen = static_cast<ZenMode*>(game.get());
    char level[32];
    std::snprintf(level, sizeof(level), "%d  (x%.1f)", zen->level, zen->score_multiplier);
    draw_score_line(x, y, w, "score", format_int(zen->score), true);
    draw_score_line(x, y + lh, w, "lines", std::to_string(zen->total_lines_cleared), false);
    draw_score_line(x, y + 2 * lh, w, "level", level, false);
    int to_next = zen->settings.lines_per_level -
                  (zen->total_lines_cleared - zen->lines_counter);
    draw_score_line(x, y + 3 * lh, w, "next", std::to_string(to_next) + " lines", false);
  } else if (mode == 2) {
    auto attack = static_cast<TimeAttackMode*>(game.get());
    double left = timer_started ? kTimeAttackLength - attack->elapsed_seconds()
                                : kTimeAttackLength;
    bool hurry = left < 15 && ui::cursor_on();
    draw_score_line(x, y, w, "time", format_time(left), !hurry);
    draw_score_line(x, y + lh, w, "score", format_int(attack->score), true);
    draw_score_line(x, y + 2 * lh, w, "lines", std::to_string(lines_this_game), false);
  } else {
    auto forty = static_cast<FirstFortyMode*>(game.get());
    int lines = std::min(forty->total_lines_cleared, kFortyLineGoal);
    double time = timer_started ? forty->elapsed_seconds() : 0;
    draw_score_line(x, y, w, "time", format_time(time), true);
    draw_score_line(x, y + lh, w, "lines", std::to_string(lines) + " / 40", false);
    // Progress bar made of characters: [########............]
    std::string bar = "[";
    for (int i = 0; i < 20; i++) bar += i < lines / 2 ? '#' : '.';
    bar += "]";
    ui::text(bar.c_str(), x, y + 2 * lh, 28, t.text);
  }

  const ScoreEntry* best = scores.best(mode);
  const ScoreEntry* last = scores.last(mode);
  draw_score_line(x, 240, w, "best", best ? format_score(mode, *best) : "---", false);
  draw_score_line(x, 276, w, "last", last ? format_score(mode, *last) : "---", false);
  DrawRectangle((int)x, 318, (int)w, 1, t.faint);

  if (scene == Scene::Playing) {
    if (ui::text_button({x, 336, w / 2 - 6, 40}, "pause", false)) pause_game();
    if (ui::text_button({x + w / 2 + 6, 336, w / 2 - 6, 40}, "menu", false)) {
      pause_game();  // the pause menu has "main menu" (and save in Zen)
      pause_menu.focus = (int)pause_menu.items.size() - 2;
    }
  }

  float ky = 400;
  ui::text("keys", x, ky, 24, t.dim);
  const char* keys[][2] = {{"left/right", "move"},   {"up / x", "rotate"},
                           {"down", "soft drop"},    {"space", "hard drop"},
                           {"esc / p", "pause"},     {"m / f11", "mute / full"}};
  for (int i = 0; i < 6; i++) {
    ui::text(keys[i][0], x + 10, ky + 28 + i * 25, 24, t.text);
    ui::text(keys[i][1], x + 160, ky + 28 + i * 25, 24, t.dim);
  }
  if (!sound::enabled()) ui::text_right("[muted]", x + w, ky, 24, t.dim);

  draw_hand_status(x, 650, false);
}

void App::draw_hand_status(float x, float y, bool right_align) {
  const ui::Theme& t = ui::theme();
  const GestureSource& g = input->gestures();
  bool live = g.connected();
  std::string label;
  if (!g.is_open()) {
    label = "hand: port busy";
  } else if (!live) {
    label = in_wsl ? "hand: start it on windows" : "hand: off";
  } else if (g.target_column() >= 0 && g.seconds_since_gesture() > 0.5) {
    label = "hand: following";
  } else if (g.seconds_since_gesture() < 2) {
    label = std::string("hand: ") + action_name(g.last_action());
    for (char& c : label) c = (char)std::tolower((unsigned char)c);
  } else {
    label = "hand: ready";
  }
  label = std::string(live ? (ui::cursor_on() ? "(*) " : "( ) ") : "( ) ") + label;
  Color c = live ? t.text : t.dim;
  if (right_align) ui::text_right(label.c_str(), x, y, 24, c);
  else ui::text(label.c_str(), x, y, 24, c);
}

// Big fading label over the board when a gesture arrives, so the player can
// see what the camera recognised
void App::draw_gesture_popup() {
  const GestureSource& g = input->gestures();
  double t = g.seconds_since_gesture();
  if (t > 0.5) return;
  float alpha = (float)(1 - t / 0.5);
  std::string name = std::string("> ") + action_name(g.last_action());
  for (char& c : name) c = (char)std::tolower((unsigned char)c);
  ui::text_centered(name.c_str(), kBoardX + 150, kBoardY + 260 - (float)t * 60, 48,
                    ColorAlpha(ui::theme().bright, alpha * 0.7f));
}

void App::spawn_line_clear(const std::vector<int>& rows, double points) {
  int cell = game->game_grid.get_cell_size();
  flash_rows = rows;
  flash_start = GetTime();
  float avg_row = 0;
  for (int row : rows) {
    avg_row += row;
    for (int i = 0; i < 22; i++) {
      Particle p;
      p.pos = {kBoardX + frand(0, 300), kBoardY + row * cell + frand(0, (float)cell)};
      p.vel = {frand(-160, 160), frand(-260, -40)};
      p.max_life = p.life = frand(0.4f, 0.9f);
      particles.push_back(p);
    }
  }
  avg_row /= rows.empty() ? 1 : rows.size();
  std::string label = "+" + format_int(points);
  if (mode == 1) {
    char mult[16];
    std::snprintf(mult, sizeof(mult), "  x%.1f", static_cast<ZenMode*>(game.get())->score_multiplier);
    label += mult;
  }
  if (rows.size() >= 4) {
    label = "4 lines! " + label;
    shake_until = GetTime() + 0.3;
  }
  if (mode != 3) {
    float_texts.push_back({label, {kBoardX + 150.f, kBoardY + avg_row * cell}, (float)GetTime()});
  }
}

void App::draw_effects() {
  const ui::Theme& t = ui::theme();
  float now = (float)GetTime();
  float dt = GetFrameTime();

  // Hard drop trails
  for (const Trail& trail : trails) {
    float age = now - trail.start;
    if (age > 0.25f) continue;
    float alpha = 0.35f * (1 - age / 0.25f);
    for (size_t i = 0; i < trail.columns.size(); i++) {
      int top = kBoardY + trail.top_rows[i] * 30;
      int bottom = kBoardY + trail.bottom_rows[i] * 30;
      DrawRectangle(kBoardX + trail.columns[i] * 30 + 8, top, 14, std::max(0, bottom - top),
                    ColorAlpha(t.text, alpha));
    }
  }
  trails.erase(std::remove_if(trails.begin(), trails.end(),
                              [now](const Trail& tr) { return now - tr.start > 0.25f; }),
               trails.end());

  // Sparks from cleared lines
  for (Particle& p : particles) {
    p.life -= dt;
    p.vel.y += 600 * dt;
    p.pos.x += p.vel.x * dt;
    p.pos.y += p.vel.y * dt;
    float a = std::max(0.0f, p.life / p.max_life);
    DrawRectangle((int)p.pos.x, (int)p.pos.y, 3, 3, ColorAlpha(t.bright, a));
  }
  particles.erase(std::remove_if(particles.begin(), particles.end(),
                                 [](const Particle& p) { return p.life <= 0; }),
                  particles.end());

  // Floating score text
  for (const FloatText& f : float_texts) {
    float age = now - f.start;
    if (age > 1.0f) continue;
    float a = age < 0.7f ? 1.0f : 1 - (age - 0.7f) / 0.3f;
    ui::text_centered(f.text.c_str(), f.pos.x, f.pos.y - age * 50, 32, ColorAlpha(t.bright, a));
  }
  float_texts.erase(std::remove_if(float_texts.begin(), float_texts.end(),
                                   [now](const FloatText& f) { return now - f.start > 1.0f; }),
                    float_texts.end());
}

void App::draw_countdown() {
  if (!counting) return;
  double left = countdown_end - GetTime();
  int beat = (int)std::ceil(left / (kCountdown / 3));
  std::string label = beat > 0 ? std::to_string(beat) : "go";
  float phase = (float)std::fmod(left, kCountdown / 3) / (kCountdown / 3);
  const ui::Theme& t = ui::theme();
  DrawRectangle(kBoardX, kBoardY + 230, 300, 120, ColorAlpha(t.background, 0.75f));
  ui::text_centered(label.c_str(), kBoardX + 150, kBoardY + 240, 96,
                    ColorAlpha(t.bright, 0.4f + 0.6f * phase));
}

// A box with a border and a title, for the pause and game over screens
void App::draw_panel(Rectangle r, const char* title) {
  const ui::Theme& t = ui::theme();
  DrawRectangle(0, 0, kWidth, kHeight, ColorAlpha(BLACK, 0.55f));
  DrawRectangleRec(r, ColorAlpha(t.background, 0.96f));
  DrawRectangleLinesEx(r, 2, t.dim);
  DrawRectangleLinesEx({r.x + 5, r.y + 5, r.width - 10, r.height - 10}, 1, t.faint);
  std::string heading = std::string("-- ") + title + " --";
  ui::text_centered(heading.c_str(), r.x + r.width / 2, r.y + 16, 40, t.bright);
}

// ------------------------------------------------------------------ sidecar

// Runs a shell command and returns the first line it prints
static std::string first_line_of(const char* command) {
  std::string line;
#ifndef _WIN32
  if (FILE* pipe = popen(command, "r")) {
    char buf[256];
    if (fgets(buf, sizeof(buf), pipe)) line = buf;
    pclose(pipe);
  }
#endif
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
  return line;
}

void App::start_sidecar() {
  if (in_wsl) {
    // WSL2 has no access to the webcam, so start the camera script on
    // Windows through WSL interop. With mirrored networking Windows reaches
    // us on localhost, otherwise it needs this WSL machine's address.
    std::string bat = std::string(GetApplicationDirectory()) + "../gesture/run_windows.bat";
    if (!FileExists(bat.c_str())) bat = "gesture/run_windows.bat";
    std::string win_path = first_line_of(("wslpath -w \"" + bat + "\" 2>/dev/null").c_str());
    std::string mode = first_line_of("wslinfo --networking-mode 2>/dev/null");
    std::string host = "127.0.0.1";
    if (mode != "mirrored") {
      std::string ips = first_line_of("hostname -I 2>/dev/null");
      host = ips.substr(0, ips.find(' '));
    }
    if (win_path.empty() || host.empty()) {
      std::cout << "\nRunning in WSL: start gesture\\run_windows.bat on Windows "
                   "(see README.md, \"Playing from WSL\")." << std::endl;
      return;
    }
    std::string cmd = "cmd.exe /c \"" + win_path + "\" --host " + host + " &";
    std::cout << "Running in WSL: starting hand tracking on Windows (sending to "
              << host << "). The first start installs Python packages." << std::endl;
    if (std::system(cmd.c_str()) != 0) std::cerr << "Failed to start sidecar" << std::endl;
    return;
  }
  // The gesture folder sits next to the executable in the Windows zip, and
  // one or two levels up in a build folder (build/ or build/Release/)
  std::string exe_dir = GetApplicationDirectory();
  std::string dirs[] = {exe_dir + "gesture/", exe_dir + "../gesture/",
                        exe_dir + "../../gesture/", "gesture/"};
  for (const std::string& dir : dirs) {
#ifdef _WIN32
    // run_windows.bat sets up Python + MediaPipe on first use
    std::string bat = dir + "run_windows.bat";
    if (!FileExists(bat.c_str())) continue;
    std::string cmd = "start \"Tetris++ hand control\" /min cmd /c \"" + bat + "\"";
#else
    std::string script = dir + "hand_control.py";
    if (!FileExists(script.c_str())) continue;
    std::string cmd = "python3 \"" + script + "\" &";
#endif
    std::cout << "Starting gesture sidecar: " << cmd << std::endl;
    if (std::system(cmd.c_str()) != 0) std::cerr << "Failed to start sidecar" << std::endl;
    return;
  }
  std::cerr << "Could not find gesture/hand_control.py" << std::endl;
}
