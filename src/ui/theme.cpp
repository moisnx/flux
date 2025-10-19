// flux/ui/theme.cpp
#include "flux/ui/theme.h"

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

namespace flux {

// Create a default theme that uses standard ncurses color pairs
// This is ONLY used when Flux runs standalone
// When embedded in Arc, the bridge overrides these values
Theme createDefaultTheme() {
  Theme theme;

  // These are just fallback indices for standalone mode
  // In embedded mode, Arc's bridge will override with proper semantic pairs
  theme.background = 0;
  theme.foreground = 0;
  theme.selected = 0;
  theme.directory = 0;
  theme.executable = 0;
  theme.hidden = 0;
  theme.symlink = 0;
  theme.parent_dir = 0;
  theme.status_bar = 0;
  theme.status_bar_active = 0;
  theme.ui_secondary = 0;
  theme.ui_border = 0;
  theme.ui_error = 0;
  theme.ui_warning = 0;
  theme.ui_accent = 0;
  theme.ui_info = 0;
  theme.ui_success = 0;

  return theme;
}

// ThemeManager - MODIFIED to not initialize color pairs when embedded
ThemeManager::ThemeManager() : embedded_mode_(false) {}

void ThemeManager::setEmbeddedMode(bool embedded) { embedded_mode_ = embedded; }

Theme ThemeManager::applyThemeDefinition(const ThemeDefinition &def) {
  Theme theme;

  // CRITICAL: In embedded mode, we DON'T call init_pair()
  // We just return indices that are already configured by the host app
  if (embedded_mode_) {
    // When embedded, the host provides the color pairs
    // This function should not be called - use the bridge instead
    return createDefaultTheme();
  }

  // Standalone mode: Initialize color pairs from scratch
  // This is only for when Flux runs independently

  if (!has_colors()) {
    return createDefaultTheme();
  }

  // Parse colors and create pairs (standalone only)
  short bg = parseColor(def.background);
  short fg = parseColor(def.foreground);

  // Initialize pairs starting from index 100 to avoid conflicts
  int next_pair = 100;

  theme.background = next_pair++;
  init_pair(theme.background, fg, bg);

  theme.foreground = next_pair++;
  init_pair(theme.foreground, fg, bg);

  theme.selected = next_pair++;
  init_pair(theme.selected, parseColor(def.state_selected), bg);

  theme.directory = next_pair++;
  init_pair(theme.directory, parseColor(def.ui_info), bg);

  theme.executable = next_pair++;
  init_pair(theme.executable, parseColor(def.ui_success), bg);

  theme.hidden = next_pair++;
  init_pair(theme.hidden, parseColor(def.state_disabled), bg);

  theme.symlink = next_pair++;
  init_pair(theme.symlink, parseColor(def.ui_accent), bg);

  theme.parent_dir = next_pair++;
  init_pair(theme.parent_dir, parseColor(def.ui_primary), bg);

  theme.status_bar = next_pair++;
  init_pair(theme.status_bar, parseColor(def.status_bar_fg),
            parseColor(def.status_bar_bg));

  theme.status_bar_active = next_pair++;
  init_pair(theme.status_bar_active, parseColor(def.status_bar_active),
            parseColor(def.status_bar_bg));

  theme.ui_secondary = next_pair++;
  init_pair(theme.ui_secondary, parseColor(def.ui_secondary), bg);

  theme.ui_border = next_pair++;
  init_pair(theme.ui_border, parseColor(def.ui_border), bg);

  theme.ui_error = next_pair++;
  init_pair(theme.ui_error, parseColor(def.ui_error), bg);

  theme.ui_warning = next_pair++;
  init_pair(theme.ui_warning, parseColor(def.ui_warning), bg);

  theme.ui_accent = next_pair++;
  init_pair(theme.ui_accent, parseColor(def.ui_accent), bg);

  theme.ui_info = next_pair++;
  init_pair(theme.ui_info, parseColor(def.ui_info), bg);

  theme.ui_success = next_pair++;
  init_pair(theme.ui_success, parseColor(def.ui_success), bg);

  return theme;
}

short ThemeManager::parseColor(const std::string &color_str) {
  // Handle special values
  if (color_str == "transparent" || color_str == "default" ||
      color_str.empty()) {
    return -1; // Use terminal default
  }

  // Parse hex colors
  if (color_str[0] == '#' && color_str.length() == 7) {
    // Extract RGB values
    int r = std::stoi(color_str.substr(1, 2), nullptr, 16);
    int g = std::stoi(color_str.substr(3, 2), nullptr, 16);
    int b = std::stoi(color_str.substr(5, 2), nullptr, 16);

    // Convert RGB (0-255) to ncurses 1000-based scale
    short r_1000 = (r * 1000) / 255;
    short g_1000 = (g * 1000) / 255;
    short b_1000 = (b * 1000) / 255;

    // Check if terminal supports custom colors
    static short next_color_id = 16; // Start after basic 16 colors

    if (COLORS >= 256 && can_change_color() && next_color_id < COLORS) {
      short color_id = next_color_id++;

      if (init_color(color_id, r_1000, g_1000, b_1000) == OK) {
        return color_id;
      }
    }

    // Fallback: Find closest basic 8-color
    return findClosestBasicColor(r, g, b);
  }

  // Named colors
  if (color_str == "black")
    return COLOR_BLACK;
  if (color_str == "red")
    return COLOR_RED;
  if (color_str == "green")
    return COLOR_GREEN;
  if (color_str == "yellow")
    return COLOR_YELLOW;
  if (color_str == "blue")
    return COLOR_BLUE;
  if (color_str == "magenta")
    return COLOR_MAGENTA;
  if (color_str == "cyan")
    return COLOR_CYAN;
  if (color_str == "white")
    return COLOR_WHITE;

  return COLOR_WHITE; // Final fallback
}

short ThemeManager::findClosestBasicColor(int r, int g, int b) {
  // Map RGB to closest basic 8 colors
  struct ColorMap {
    short ncurses_color;
    int r, g, b;
  };

  static const ColorMap colors[] = {
      {COLOR_BLACK, 0, 0, 0},    {COLOR_RED, 255, 0, 0},
      {COLOR_GREEN, 0, 255, 0},  {COLOR_YELLOW, 255, 255, 0},
      {COLOR_BLUE, 0, 0, 255},   {COLOR_MAGENTA, 255, 0, 255},
      {COLOR_CYAN, 0, 255, 255}, {COLOR_WHITE, 255, 255, 255}};

  double min_distance = 1000000;
  short closest = COLOR_WHITE;

  for (const auto &color : colors) {
    double dr = r - color.r;
    double dg = g - color.g;
    double db = b - color.b;
    double distance = dr * dr + dg * dg + db * db;

    if (distance < min_distance) {
      min_distance = distance;
      closest = color.ncurses_color;
    }
  }

  return closest;
}

ThemeDefinition ThemeManager::getDefaultThemeDef()
{
  return ThemeDefinition{
      .name = "default",
      .background = "transparent",
      .foreground = "#C9D1D9",
      .selected = "#264F78",
      .directory = "#79C0FF",
      .executable = "#7EE787",
      .hidden = "#6E7681",
      .symlink = "#D2A8FF",
      .parent_dir = "#58A6FF",
      .state_active = "#58A6FF",
      .state_selected = "#264F78",
      .state_hover = "#161B22",
      .state_disabled = "#6E7681",
      .ui_primary = "#58A6FF",
      .ui_secondary = "#8B949E",
      .ui_accent = "#D2A8FF",
      .ui_success = "#7EE787",
      .ui_warning = "#E3B341",
      .ui_error = "#FF7B72",
      .ui_info = "#79C0FF",
      .ui_border = "#30363D",
      .status_bar_bg = "#21262D",
      .status_bar_fg = "#C9D1D9",
      .status_bar_active = "#58A6FF",
  };
}
} // namespace flux
  //   return -1; // Use terminal default
  // }

// if (color_