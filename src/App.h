#pragma once
#include <memory>
#include <string>
#include <vector>

#include "game.h"
#include "input/InputManager.h"
#include "raylib.h"
#include "ui/Widgets.h"

// Owns the single game window and switches between screens (scenes).
class App {
 public:
  int mode = 0;
  // 0-main menu
  // 1 - Zen Mode
  // 2 - Time Attack
  // 3 - First 40 Lines

  // Opens the window and runs until the player quits
  void run_menu();

  // Starts the gesture sidecar (gesture/hand_control.py) in the background
  bool launch_gesture_sidecar = false;

 private:
  enum class Scene { MainMenu, ZenChoice, Playing, Paused, GameOver };

  // Size of the virtual screen; it is scaled to fit the real window
  static const int kWidth = 800;
  static const int kHeight = 700;
  static const int kBoardX = 70;
  static const int kBoardY = 50;
  static const int kSideX = 430;

  Scene scene = Scene::MainMenu;
  bool quit = false;
  std::unique_ptr<Game> game;
  std::unique_ptr<InputManager> input;

  ui::Menu main_menu;
  ui::Menu zen_menu;
  ui::Menu pause_menu;
  ui::Menu over_menu;

  // Line clear flash
  int seen_clear_events = 0;
  std::vector<int> flash_rows;
  double flash_start = -10;

  // Decorative blocks drifting down behind the menus
  struct FallingCell {
    float x, y, speed;
    int color_id;
    int size;
  };
  std::vector<FallingCell> background_cells;

  void start_game(int new_mode, bool load_saved);
  void frame(const std::vector<Action>& actions);
  void update_game(const std::vector<Action>& actions);
  void pause_game();
  void resume_game();
  void end_game();

  void draw_background(bool animate);
  void draw_title(float y);
  void draw_board();
  void draw_sidebar();
  void draw_stat(float y, const char* label, const std::string& value,
                 Color accent);
  void draw_gesture_status(float x, float y, float width);
  void draw_gesture_popup();
  void draw_overlay_panel(const char* title, const std::string& subtitle,
                          Color accent, float height);
  std::string game_over_title();
  std::string game_over_subtitle();
  void start_sidecar();
};
