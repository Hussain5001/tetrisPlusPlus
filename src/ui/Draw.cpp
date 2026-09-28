#include "Draw.h"

#include <cmath>
#include <string>

namespace ui {

namespace {
Font g_font;
bool g_font_loaded = false;
int g_phosphor = kAmber;

Theme make_theme(Color phosphor) {
  Theme t;
  t.background = {9, 11, 9, 255};
  t.bright = ColorBrightness(phosphor, 0.25f);
  t.text = phosphor;
  t.dim = ColorAlpha(phosphor, 0.55f);
  t.faint = ColorAlpha(phosphor, 0.16f);
  t.alert = {255, 92, 64, 255};
  return t;
}

const Color kAmberColor = {255, 176, 46, 255};
const Color kGreenColor = {88, 255, 128, 255};

Theme g_themes[] = {make_theme(kAmberColor), make_theme(kGreenColor),
                    make_theme(kAmberColor)};
}  // namespace

void set_phosphor(int p) { g_phosphor = (p >= 0 && p <= 2) ? p : kAmber; }
int phosphor() { return g_phosphor; }
const Theme& theme() { return g_themes[g_phosphor]; }

const char* phosphor_name(int p) {
  switch (p) {
    case kGreen: return "green";
    case kMulti: return "multi";
    default: return "amber";
  }
}

Color block_color(int id) {
  if (id <= 0) return theme().faint;
  id = 1 + (id - 1) % 7;
  if (g_phosphor == kMulti) {
    // Muted, slightly dusty versions of the classic colours
    static const Color muted[] = {{96, 196, 190, 255},  {104, 132, 214, 255},
                                  {222, 142, 70, 255},  {226, 198, 96, 255},
                                  {120, 190, 104, 255}, {172, 118, 200, 255},
                                  {212, 96, 84, 255}};
    return muted[id - 1];
  }
  // One phosphor: tell the blocks apart by brightness
  static const float shade[] = {0.0f, -0.35f, 0.12f, -0.18f, 0.25f, -0.45f, -0.08f};
  return ColorBrightness(theme().text, shade[id - 1]);
}

void draw_cell(int x, int y, int size, int color_id, float alpha) {
  Color c = block_color(color_id);
  // Soft outer glow, bright edge, dark core, bright centre pip
  DrawRectangle(x, y, size, size, ColorAlpha(c, 0.10f * alpha));
  DrawRectangle(x + 2, y + 2, size - 4, size - 4, ColorAlpha(c, 0.95f * alpha));
  int core = size / 4;
  DrawRectangle(x + core, y + core, size - 2 * core, size - 2 * core,
                ColorAlpha(ColorBrightness(c, -0.55f), alpha));
  int pip = size / 10 + 1;
  DrawRectangle(x + size / 2 - pip / 2, y + size / 2 - pip / 2, pip, pip,
                ColorAlpha(c, 0.8f * alpha));
}

void draw_ghost_cell(int x, int y, int size, int color_id) {
  Color c = ColorAlpha(block_color(color_id), 0.45f);
  float fs = (float)size;
  float w = text_width("[]", fs);
  text("[]", x + (size - w) / 2, (float)y, fs, c);
}

void draw_empty_cell(int x, int y, int size) {
  DrawRectangle(x + size / 2 - 1, y + size / 2 - 1, 2, 2, theme().faint);
}

Font& font() {
  if (!g_font_loaded) g_font = GetFontDefault();
  return g_font;
}

void load_font() {
  // Look next to the executable first (CMake copies assets there), then in
  // the current working directory.
  std::string candidates[] = {
      std::string(GetApplicationDirectory()) + "assets/monogram.ttf",
      "assets/monogram.ttf", "../assets/monogram.ttf"};
  for (const std::string& path : candidates) {
    if (FileExists(path.c_str())) {
      g_font = LoadFontEx(path.c_str(), 64, 0, 0);
      SetTextureFilter(g_font.texture, TEXTURE_FILTER_POINT);
      g_font_loaded = true;
      return;
    }
  }
  TraceLog(LOG_WARNING, "monogram.ttf not found, using default font");
}

void unload_font() {
  if (g_font_loaded) UnloadFont(g_font);
  g_font_loaded = false;
}

void text(const char* str, float x, float y, float size, Color color) {
  DrawTextEx(font(), str, {std::floor(x), std::floor(y)}, size, 1, color);
}

void text_centered(const char* str, float cx, float y, float size, Color color) {
  text(str, cx - text_width(str, size) / 2, y, size, color);
}

void text_right(const char* str, float right_x, float y, float size, Color color) {
  text(str, right_x - text_width(str, size), y, size, color);
}

float text_width(const char* str, float size) {
  return MeasureTextEx(font(), str, size, 1).x;
}

void draw_logo(float cx, float y, int cell, float time) {
  // 3x5 block letters
  static const char* T[] = {"###", ".#.", ".#.", ".#.", ".#."};
  static const char* E[] = {"###", "#..", "##.", "#..", "###"};
  static const char* R[] = {"##.", "#.#", "##.", "#.#", "#.#"};
  static const char* I[] = {"###", ".#.", ".#.", ".#.", "###"};
  static const char* S[] = {"###", "#..", "###", "..#", "###"};
  static const char* P[] = {"...", ".#.", "###", ".#.", "..."};
  const char** letters[] = {T, E, T, R, I, S, P, P};
  const int count = 8;
  int width = count * 4 - 1;
  float x0 = cx - width * cell / 2.0f;
  for (int l = 0; l < count; l++) {
    for (int r = 0; r < 5; r++) {
      for (int c = 0; c < 3; c++) {
        if (letters[l][r][c] != '#') continue;
        int col = l * 4 + c;
        // A slow brightness wave runs across the logo
        float wave = 0.75f + 0.25f * std::sin(time * 2.2f - col * 0.35f);
        draw_cell((int)(x0 + col * cell), (int)(y + r * cell), cell - 1,
                  g_phosphor == kMulti ? l % 7 + 1 : 1, wave);
      }
    }
  }
}

bool cursor_on() { return std::fmod(GetTime(), 1.0) < 0.55; }

namespace {
bool g_mouse_active = false;
int g_mouse_frames = 0;
Vector2 g_mouse_start = {0, 0};
}  // namespace

void update_mouse() {
  if (g_mouse_active) return;
  Vector2 m = GetMousePosition();
  if (++g_mouse_frames <= 5) {
    g_mouse_start = m;  // ignore the first frames while the window settles
  } else if (m.x != g_mouse_start.x || m.y != g_mouse_start.y ||
             IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    g_mouse_active = true;
  }
}

bool mouse_active() { return g_mouse_active; }

}  // namespace ui
