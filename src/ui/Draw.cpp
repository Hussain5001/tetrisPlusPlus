#include "Draw.h"

#include <string>

namespace ui {

namespace {
Font g_font;
bool g_font_loaded = false;
}  // namespace

void draw_cell(int x, int y, int size, Color color) {
  Color light = ColorBrightness(color, 0.35f);
  Color dark = ColorBrightness(color, -0.35f);
  int bevel = size / 7;
  DrawRectangle(x, y, size, size, dark);
  // Top and left highlight
  DrawRectangle(x, y, size, bevel, light);
  DrawRectangle(x, y, bevel, size, light);
  // Face
  DrawRectangle(x + bevel, y + bevel, size - 2 * bevel, size - 2 * bevel, color);
  // Small shine in the top-left corner
  DrawRectangle(x + bevel, y + bevel, bevel, bevel, ColorAlpha(WHITE, 0.6f));
}

void draw_ghost_cell(int x, int y, int size, Color color) {
  DrawRectangle(x + 2, y + 2, size - 4, size - 4, ColorAlpha(color, 0.12f));
  DrawRectangleLinesEx({(float)x + 2, (float)y + 2, (float)size - 4, (float)size - 4},
                       2, ColorAlpha(color, 0.55f));
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
  DrawTextEx(font(), str, {x, y}, size, 1, color);
}

void text_centered(const char* str, float cx, float y, float size, Color color) {
  text(str, cx - text_width(str, size) / 2, y, size, color);
}

float text_width(const char* str, float size) {
  return MeasureTextEx(font(), str, size, 1).x;
}

}  // namespace ui
