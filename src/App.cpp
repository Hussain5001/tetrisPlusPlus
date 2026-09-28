#include "App.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "Color.h"
#include "first_forty_mode.h"
#include "time_attack_mode.h"
#include "time_dependent_mode.h"
#include "ui/Draw.h"
#include "zen_mode.h"

namespace {
const Color kZenColor = {66, 110, 235, 255};
const Color kAttackColor = {95, 210, 95, 255};
const Color kFortyColor = {235, 70, 85, 255};
const Color kQuitColor = {140, 146, 180, 255};

const char* kModeNames[] = {"", "ZEN MODE", "TIME ATTACK", "FIRST 40 LINES"};
const char* kModeHints[] = {
    "", "Play at your own pace. Save and continue later.",
    "Score as much as you can in 120 seconds.",
    "Clear 40 lines as fast as possible."};

const double kTimeAttackLength = 120.0;
const int kFortyLineGoal = 40;

std::string format_time(double seconds) {
  if (seconds < 0) seconds = 0;
  char buf[32];
  int minutes = (int)seconds / 60;
  std::snprintf(buf, sizeof(buf), "%d:%04.1f", minutes,
                seconds - minutes * 60);
  return buf;
}

std::string format_int(double value) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.0f", value);
  return buf;
}
}  // namespace

void App::run_menu() {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(kWidth, kHeight, "Tetris++");
  SetWindowMinSize(kWidth / 2, kHeight / 2);
  SetExitKey(KEY_NULL);  // Esc pauses instead of closing the window
  SetTargetFPS(60);
  ui::load_font();

  RenderTexture2D target = LoadRenderTexture(kWidth, kHeight);
  SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

  input.reset(new InputManager());
  if (launch_gesture_sidecar) start_sidecar();

  main_menu = ui::Menu({"ZEN MODE", "TIME ATTACK", "FIRST 40 LINES", "QUIT"});
  main_menu.accents = {kZenColor, kAttackColor, kFortyColor, kQuitColor};
  zen_menu = ui::Menu({"NEW GAME", "CONTINUE SAVED", "BACK"});
  zen_menu.accents = {kZenColor, kAttackColor, kQuitColor};
  over_menu = ui::Menu({"PLAY AGAIN", "MAIN MENU", "QUIT GAME"});

  while (!WindowShouldClose() && !quit) {
    // Scale the virtual screen to the window, keeping its aspect ratio
    float scale = std::min((float)GetScreenWidth() / kWidth,
                           (float)GetScreenHeight() / kHeight);
    float offset_x = (GetScreenWidth() - kWidth * scale) / 2;
    float offset_y = (GetScreenHeight() - kHeight * scale) / 2;
    SetMouseOffset((int)-offset_x, (int)-offset_y);
    SetMouseScale(1 / scale, 1 / scale);

    std::vector<Action> actions = input->poll();

    BeginTextureMode(target);
    ClearBackground(ui::kBackground);
    frame(actions);
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(target.texture,
                   {0, 0, (float)kWidth, (float)-kHeight},
                   {offset_x, offset_y, kWidth * scale, kHeight * scale},
                   {0, 0}, 0, WHITE);
    EndDrawing();
  }

  game.reset();
  input.reset();
  UnloadRenderTexture(target);
  ui::unload_font();
  CloseWindow();
}

// Updates and draws one frame of the current scene
void App::frame(const std::vector<Action>& actions) {
  switch (scene) {
    case Scene::MainMenu: {
      draw_background(true);
      draw_title(90);
      int choice = main_menu.run(actions, kWidth / 2, 280);
      if (main_menu.focus < 3) {
        ui::text_centered(kModeHints[main_menu.focus + 1], kWidth / 2, 570, 26,
                          ui::kTextDim);
      }
      draw_gesture_status(kWidth / 2 - 190, 620, 380);
      if (choice == 0) {
        zen_menu.focus = 0;
        scene = Scene::ZenChoice;
      } else if (choice == 1 || choice == 2) {
        start_game(choice + 1, false);
      } else if (choice == 3) {
        quit = true;
      }
      break;
    }

    case Scene::ZenChoice: {
      draw_background(true);
      draw_title(90);
      ui::text_centered("ZEN MODE", kWidth / 2, 230, 40, kZenColor);
      int choice = zen_menu.run(actions, kWidth / 2, 300);
      draw_gesture_status(kWidth / 2 - 190, 620, 380);
      for (Action a : actions) {
        if (a == Action::Back || a == Action::Pause) choice = 2;
      }
      if (choice == 0) {
        start_game(1, false);
      } else if (choice == 1) {
        start_game(1, true);
      } else if (choice == 2) {
        scene = Scene::MainMenu;
      }
      break;
    }

    case Scene::Playing: {
      update_game(actions);
      if (!game) break;
      draw_background(false);
      draw_board();
      draw_sidebar();
      draw_gesture_popup();
      break;
    }

    case Scene::Paused: {
      draw_background(false);
      draw_board();
      draw_sidebar();
      draw_overlay_panel("PAUSED", "Swipe to choose, palm to select",
                         ui::kAccent, 380);
      int choice = pause_menu.run(actions, kWidth / 2, 290, 300, 50, 12);
      for (Action a : actions) {
        if (a == Action::Pause || a == Action::Back) choice = 0;
      }
      const std::string picked =
          choice >= 0 ? pause_menu.items[choice] : std::string();
      if (picked == "RESUME") {
        resume_game();
      } else if (picked == "SAVE & QUIT") {
        static_cast<ZenMode*>(game.get())->save_game_state();
        quit = true;
      } else if (picked == "MAIN MENU") {
        game.reset();
        scene = Scene::MainMenu;
      } else if (picked == "QUIT GAME") {
        quit = true;
      }
      break;
    }

    case Scene::GameOver: {
      draw_background(false);
      draw_board();
      draw_sidebar();
      std::string title = game_over_title();
      bool good = title != "GAME OVER";
      draw_overlay_panel(title.c_str(), game_over_subtitle(),
                         good ? kAttackColor : kFortyColor, 360);
      int choice = over_menu.run(actions, kWidth / 2, 330, 300, 50, 12);
      if (choice == 0) {
        start_game(mode, false);
      } else if (choice == 1) {
        game.reset();
        scene = Scene::MainMenu;
      } else if (choice == 2) {
        quit = true;
      }
      break;
    }
  }
}

void App::start_game(int new_mode, bool load_saved) {
  mode = new_mode;
  game.reset();  // destroy the old game before building the new one
  if (mode == 1) {
    ZenMode* zen = new ZenMode();
    if (load_saved) zen->load_game_state();
    game.reset(zen);
  } else if (mode == 2) {
    TimeAttackMode* attack = new TimeAttackMode();
    attack->game_start();
    game.reset(attack);
  } else {
    FirstFortyMode* forty = new FirstFortyMode();
    forty->game_start();
    game.reset(forty);
  }
  game->fall_start = GetTime();
  seen_clear_events = 0;
  flash_rows.clear();

  if (mode == 1) {
    pause_menu = ui::Menu({"RESUME", "SAVE & QUIT", "MAIN MENU", "QUIT GAME"});
  } else {
    pause_menu = ui::Menu({"RESUME", "MAIN MENU", "QUIT GAME"});
  }
  over_menu.focus = 0;
  scene = Scene::Playing;
}

void App::update_game(const std::vector<Action>& actions) {
  for (Action a : actions) {
    if (a == Action::Pause || a == Action::Confirm) {
      pause_game();
      return;
    }
    game->apply(a);
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

  // Start a flash when rows were cleared
  if (game->game_grid.clear_events != seen_clear_events) {
    seen_clear_events = game->game_grid.clear_events;
    flash_rows = game->game_grid.last_cleared_rows;
    flash_start = GetTime();
  }

  if (!game->game_over && game->is_game_finished()) {
    game->game_over = true;
  }
  if (game->game_over) end_game();
}

void App::pause_game() {
  if (auto timed = dynamic_cast<TimeDependentMode*>(game.get())) {
    timed->pause_timer();
  }
  pause_menu.focus = 0;
  scene = Scene::Paused;
}

void App::resume_game() {
  if (auto timed = dynamic_cast<TimeDependentMode*>(game.get())) {
    timed->resume_timer();
  }
  game->fall_start = GetTime();  // don't drop immediately after resuming
  scene = Scene::Playing;
}

void App::end_game() {
  if (auto timed = dynamic_cast<TimeDependentMode*>(game.get())) {
    if (timed->timer_on) timed->game_end();
  }
  over_menu.focus = 0;
  scene = Scene::GameOver;
}

std::string App::game_over_title() {
  if (mode == 2) {
    auto attack = static_cast<TimeAttackMode*>(game.get());
    if (attack->elapsed_seconds() >= kTimeAttackLength) return "TIME'S UP!";
  }
  if (mode == 3) {
    auto forty = static_cast<FirstFortyMode*>(game.get());
    if (forty->total_lines_cleared >= kFortyLineGoal) return "COMPLETE!";
  }
  return "GAME OVER";
}

std::string App::game_over_subtitle() {
  if (mode == 3) {
    auto forty = static_cast<FirstFortyMode*>(game.get());
    return "Lines " + std::to_string(forty->total_lines_cleared) + "   Time " +
           format_time(forty->elapsed_seconds());
  }
  return "Final score  " + format_int(game->score);
}

// ---------------------------------------------------------------- drawing

void App::draw_background(bool animate) {
  if (background_cells.empty()) {
    for (int i = 0; i < 40; i++) {
      background_cells.push_back(
          {(float)GetRandomValue(0, kWidth), (float)GetRandomValue(-kHeight, kHeight),
           (float)GetRandomValue(20, 70), GetRandomValue(1, 7),
           GetRandomValue(14, 30)});
    }
  }
  float dt = animate ? GetFrameTime() : 0;
  for (FallingCell& c : background_cells) {
    c.y += c.speed * dt;
    if (c.y > kHeight) {
      c.y = (float)-c.size;
      c.x = (float)GetRandomValue(0, kWidth);
    }
    ui::draw_cell((int)c.x, (int)c.y, c.size,
                  ColorBrightness(cell_color(c.color_id), -0.72f));
  }
}

void App::draw_title(float y) {
  const char* title = "TETRIS++";
  float size = 110;
  float x = kWidth / 2 - ui::text_width(title, size) / 2;
  float t = (float)GetTime();
  char letter[2] = {0, 0};
  for (int i = 0; title[i]; i++) {
    letter[0] = title[i];
    float bob = std::sin(t * 2.5f + i * 0.6f) * 5;
    Color c = cell_color(i % 7 + 1);
    ui::text(letter, x + 4, y + bob + 5, size, ColorAlpha(BLACK, 0.5f));
    ui::text(letter, x, y + bob, size, c);
    x += ui::text_width(letter, size) + 1;
  }
  ui::text_centered("hands-free edition", kWidth / 2, y + 100, 28,
                    ui::kTextDim);
}

void App::draw_board() {
  int cell = game->game_grid.get_cell_size();
  int width = game->game_grid.get_num_cols() * cell;
  int height = game->game_grid.get_num_rows() * cell;

  // Frame around the well
  Rectangle frame = {(float)kBoardX - 10, (float)kBoardY - 10,
                     (float)width + 20, (float)height + 20};
  DrawRectangleRounded(frame, 0.04f, 8, ui::kPanel);
  DrawRectangleRoundedLines(frame, 0.04f, 8, 2, ui::kPanelLight);

  game->display(kBoardX, kBoardY);

  // Line clear flash
  float t = (float)(GetTime() - flash_start);
  if (t < 0.3f) {
    for (int row : flash_rows) {
      DrawRectangle(kBoardX, kBoardY + row * cell, width, cell,
                    ColorAlpha(WHITE, 0.8f * (1 - t / 0.3f)));
    }
  }
}

void App::draw_stat(float y, const char* label, const std::string& value,
                    Color accent) {
  Rectangle r = {(float)kSideX, y, 330, 78};
  DrawRectangleRounded(r, 0.2f, 8, ui::kPanel);
  DrawRectangle(kSideX, (int)y + 14, 4, 50, accent);
  ui::text(label, kSideX + 20, y + 8, 24, ui::kTextDim);
  ui::text(value.c_str(), kSideX + 20, y + 30, 44, ui::kText);
}

void App::draw_sidebar() {
  Color mode_color = mode == 1 ? kZenColor : mode == 2 ? kAttackColor : kFortyColor;
  ui::text(kModeNames[mode], kSideX, kBoardY - 8, 44, mode_color);

  float y = kBoardY + 50;
  if (mode == 1) {
    auto zen = static_cast<ZenMode*>(game.get());
    char speed[16];
    std::snprintf(speed, sizeof(speed), "x%.1f", zen->score_multiplier);
    draw_stat(y, "SCORE", format_int(zen->score), kZenColor);
    draw_stat(y + 90, "LINES", std::to_string(zen->total_lines_cleared), kAttackColor);
    draw_stat(y + 180, "SPEED", speed, kFortyColor);
  } else if (mode == 2) {
    auto attack = static_cast<TimeAttackMode*>(game.get());
    double left = kTimeAttackLength - attack->elapsed_seconds();
    bool blink = left < 15 && std::fmod(GetTime(), 0.5) < 0.25;
    draw_stat(y, "TIME LEFT", format_time(left), blink ? RED : kAttackColor);
    draw_stat(y + 90, "SCORE", format_int(attack->score), kZenColor);
  } else {
    auto forty = static_cast<FirstFortyMode*>(game.get());
    int lines = std::min(forty->total_lines_cleared, kFortyLineGoal);
    draw_stat(y, "LINES", std::to_string(lines) + " / 40", kFortyColor);
    // Progress bar under the lines counter
    DrawRectangle(kSideX + 20, (int)y + 70, 290, 4, ui::kPanelLight);
    DrawRectangle(kSideX + 20, (int)y + 70, 290 * lines / kFortyLineGoal, 4, kFortyColor);
    draw_stat(y + 90, "TIME", format_time(forty->elapsed_seconds()), kZenColor);
  }

  // Buttons
  float by = kBoardY + 330;
  bool clickable = scene == Scene::Playing;
  if (ui::button({(float)kSideX, by, 160, 48}, "PAUSE", false, ui::kAccent, 28) &&
      clickable) {
    pause_game();
  }
  if (ui::button({(float)kSideX + 170, by, 160, 48}, "MENU", false, kQuitColor, 28) &&
      clickable) {
    pause_game();  // the pause menu offers "main menu" (and save in Zen)
    pause_menu.focus = (int)pause_menu.items.size() - 2;
  }

  // Controls
  float cy = by + 70;
  ui::text("CONTROLS", kSideX, cy, 24, ui::kTextDim);
  const char* keys[][2] = {{"LEFT / RIGHT", "move"},
                           {"UP / X", "rotate"},
                           {"DOWN", "soft drop"},
                           {"SPACE", "hard drop"},
                           {"ESC / P", "pause"}};
  for (int i = 0; i < 5; i++) {
    ui::text(keys[i][0], kSideX, cy + 28 + i * 24, 24, ui::kText);
    ui::text(keys[i][1], kSideX + 170, cy + 28 + i * 24, 24, ui::kTextDim);
  }

  draw_gesture_status(kSideX, kBoardY + 556, 330);
}

void App::draw_gesture_status(float x, float y, float width) {
  const GestureSource& g = input->gestures();
  Rectangle r = {x, y, width, 44};
  DrawRectangleRounded(r, 0.3f, 8, ui::kPanel);
  bool live = g.connected();
  Color dot = !g.is_open() ? RED : live ? Color{95, 210, 95, 255} : ui::kTextDim;
  float pulse = live ? 1 + 0.25f * std::sin((float)GetTime() * 6) : 1;
  DrawCircle((int)x + 22, (int)y + 22, 7 * pulse, dot);

  std::string label;
  if (!g.is_open()) {
    label = "HAND CONTROL: port busy";
  } else if (!live) {
    label = "HAND CONTROL: off (--gestures)";
  } else if (g.target_column() >= 0 && g.seconds_since_gesture() > 0.5) {
    label = "HAND CONTROL: following hand";
  } else if (g.seconds_since_gesture() < 2) {
    label = std::string("HAND CONTROL: ") + action_name(g.last_action());
  } else {
    label = "HAND CONTROL: ready";
  }
  ui::text(label.c_str(), x + 40, y + 10, 24, live ? ui::kText : ui::kTextDim);
}

// Big fading label over the board when a gesture arrives, so the player can
// see what the camera recognised
void App::draw_gesture_popup() {
  const GestureSource& g = input->gestures();
  double t = g.seconds_since_gesture();
  if (t > 0.5) return;
  float alpha = (float)(1 - t / 0.5);
  const char* name = action_name(g.last_action());
  float cx = kBoardX + 150;
  ui::text_centered(name, cx, kBoardY + 250 - (float)t * 60, 56,
                    ColorAlpha(ui::kAccent, alpha * 0.8f));
}

void App::draw_overlay_panel(const char* title, const std::string& subtitle,
                             Color accent, float height) {
  DrawRectangle(0, 0, kWidth, kHeight, ColorAlpha(BLACK, 0.6f));
  Rectangle panel = {kWidth / 2 - 220.f, 150, 440, height};
  DrawRectangleRounded(panel, 0.08f, 8, ui::kPanel);
  DrawRectangleRoundedLines(panel, 0.08f, 8, 3, accent);
  ui::text_centered(title, kWidth / 2, 170, 72, accent);
  ui::text_centered(subtitle.c_str(), kWidth / 2, 245, 28, ui::kText);
}

void App::start_sidecar() {
  std::string candidates[] = {
      std::string(GetApplicationDirectory()) + "../gesture/hand_control.py",
      "gesture/hand_control.py", "../gesture/hand_control.py"};
  for (const std::string& path : candidates) {
    if (!FileExists(path.c_str())) continue;
#ifdef _WIN32
    std::string cmd = "start \"\" python \"" + path + "\"";
#else
    std::string cmd = "python3 \"" + path + "\" &";
#endif
    std::cout << "Starting gesture sidecar: " << cmd << std::endl;
    if (std::system(cmd.c_str()) != 0) std::cerr << "Failed to start sidecar" << std::endl;
    return;
  }
  std::cerr << "Could not find gesture/hand_control.py" << std::endl;
}
