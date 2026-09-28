#pragma once
#include <memory>
#include <string>
#include <vector>

#include "HighScores.h"
#include "Profile.h"
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

  // Listen for gestures on all network interfaces (--gesture-bind 0.0.0.0)
  bool gesture_bind_any = false;

 private:
  enum class Scene { Boot, MainMenu, ZenSetup, Playing, Paused, GameOver };

  // Size of the virtual screen; it is scaled to fit the real window
  static const int kWidth = 800;
  static const int kHeight = 700;
  static const int kBoardX = 96;
  static const int kBoardY = 30;
  static const int kSideX = 460;
  static const int kSideW = 310;

  Scene scene = Scene::Boot;
  bool quit = false;
  std::unique_ptr<Game> game;
  std::unique_ptr<InputManager> input;
  Profile profile;
  HighScores scores;

  ui::Menu main_menu;
  ui::Menu zen_menu;
  ui::Menu pause_menu;
  ui::Menu over_menu;

  // CRT screen effect
  Shader crt_shader{};
  bool crt_loaded = false;
  int crt_time_loc = -1;
  int crt_strength_loc = -1;

  // Boot screen
  double boot_start = 0;
  std::vector<std::string> boot_lines;

  // Countdown before play starts or resumes
  double countdown_end = 0;
  bool counting = false;
  bool timer_started = false;
  int last_countdown_beep = -1;

  // Values seen last frame, used to spot events for sounds and effects
  int seen_clear_events = 0;
  int seen_piece_id = 0;
  int seen_level = 0;
  int lines_this_game = 0;

  // Result of the last finished game
  ScoreEntry last_result;
  int last_rank = 0;
  bool last_eligible = false;

  // Smoothly sliding falling block
  Vector2 piece_pos = {0, 0};
  int drawn_piece_id = -1;

  // Effects
  struct Particle {
    Vector2 pos, vel;
    float life, max_life;
  };
  struct FloatText {
    std::string text;
    Vector2 pos;
    float start;
  };
  struct Trail {
    std::vector<int> columns;
    std::vector<int> top_rows;
    std::vector<int> bottom_rows;
    float start;
  };
  struct RainColumn {
    float y, speed;
    std::string chars;
  };
  std::vector<Particle> particles;
  std::vector<FloatText> float_texts;
  std::vector<Trail> trails;
  std::vector<RainColumn> rain;
  std::vector<int> flash_rows;
  double flash_start = -10;
  double shake_until = 0;
  bool was_focused = true;

  void frame(const std::vector<Action>& actions);
  void handle_global_keys();

  // Scenes
  void boot_scene(const std::vector<Action>& actions);
  void main_menu_scene(const std::vector<Action>& actions);
  void zen_setup_scene(const std::vector<Action>& actions);
  void pause_scene(const std::vector<Action>& actions);
  void game_over_scene(const std::vector<Action>& actions);

  // Game flow
  void start_game(int new_mode, bool load_saved);
  void update_game(const std::vector<Action>& actions);
  void begin_countdown();
  void pause_game();
  void resume_game();
  void end_game();
  void apply_profile();
  void refresh_zen_menu();

  // Drawing
  void draw_rain(bool animate);
  void draw_board();
  void draw_sidebar();
  void draw_effects();
  void draw_countdown();
  void draw_hand_status(float x, float y, bool right_align);
  void draw_gesture_popup();
  void draw_panel(Rectangle r, const char* title);
  void draw_score_line(float x, float y, float width, const char* label,
                       const std::string& value, bool bright);
  void spawn_line_clear(const std::vector<int>& rows, double points);

  std::string format_score(int for_mode, const ScoreEntry& entry) const;
  std::string game_over_title();
  void start_sidecar();

  // WSL can't use the laptop webcam, so the camera script runs on Windows
  bool in_wsl = false;
};
