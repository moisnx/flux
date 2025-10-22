#include "include/ui/theme.hpp"

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

namespace fx {

Theme createDefaultTheme() {
  Theme theme;

  // Default RGB colors (GitHub Dark theme)
  theme.background = 0x0D1117;
  theme.foreground = 0xC9D1D9;
  theme.selected = 0x264F78;
  theme.directory = 0x79C0FF;
  theme.executable = 0x7EE787;
  theme.hidden = 0x6E7681;
  theme.symlink = 0xD2A8FF;
  theme.parent_dir = 0x58A6FF;
  theme.status_bar = 0x21262D;
  theme.status_bar_active = 0x58A6FF;
  theme.ui_secondary = 0x8B949E;
  theme.ui_border = 0x30363D;
  theme.ui_error = 0xFF7B72;
  theme.ui_warning = 0xE3B341;
  theme.ui_accent = 0xD2A8FF;
  theme.ui_info = 0x79C0FF;
  theme.ui_success = 0x7EE787;

  return theme;
}

// ThemeManager - MODIFIED to not initialize color pairs when embedded
ThemeManager::ThemeManager() {}

Theme ThemeManager::applyThemeDefinition(const ThemeDefinition &def) {
  Theme theme;

  // Simply parse hex colors - NO init_pair() calls!
  theme.background = parseHexColor(def.background);
  theme.foreground = parseHexColor(def.foreground);
  theme.selected = parseHexColor(def.selected);
  theme.directory = parseHexColor(def.directory);
  theme.executable = parseHexColor(def.executable);
  theme.hidden = parseHexColor(def.hidden);
  theme.symlink = parseHexColor(def.symlink);
  theme.parent_dir = parseHexColor(def.parent_dir);

  // For colors that need composition later
  theme.status_bar = parseHexColor(def.status_bar_bg);
  theme.status_bar_active = parseHexColor(def.status_bar_active);

  theme.ui_secondary = parseHexColor(def.ui_secondary);
  theme.ui_border = parseHexColor(def.ui_border);
  theme.ui_error = parseHexColor(def.ui_error);
  theme.ui_warning = parseHexColor(def.ui_warning);
  theme.ui_accent = parseHexColor(def.ui_accent);
  theme.ui_info = parseHexColor(def.ui_info);
  theme.ui_success = parseHexColor(def.ui_success);

  return theme;
}

uint32_t ThemeManager::parseHexColor(const std::string &color_str) {
  // Handle special values
  if (color_str == "transparent" || color_str == "default" ||
      color_str.empty()) {
    return 0x000000;
  }

  // Parse hex colors (#RRGGBB)
  if (color_str[0] == '#' && color_str.length() == 7) {
    try {
      // Parse as-is: this gives us 0xRRGGBB
      uint32_t rgb = std::stoul(color_str.substr(1), nullptr, 16);

      // Extract components
      uint8_t r = (rgb >> 16) & 0xFF;
      uint8_t g = (rgb >> 8) & 0xFF;
      uint8_t b = rgb & 0xFF;

      // Store in the SAME format for consistency
      return (r << 16) | (g << 8) | b; // 0xRRGGBB

    } catch (...) {
      return 0xFFFFFF;
    }
  }

  // Named colors - store as RGB
  if (color_str == "black")
    return 0x000000;
  if (color_str == "red")
    return 0xFF0000;
  if (color_str == "green")
    return 0x00FF00;
  if (color_str == "yellow")
    return 0xFFFF00;
  if (color_str == "blue")
    return 0x0000FF;
  if (color_str == "magenta")
    return 0xFF00FF;
  if (color_str == "cyan")
    return 0x00FFFF;
  if (color_str == "white")
    return 0xFFFFFF;

  return 0xFFFFFF;
}

ThemeDefinition ThemeManager::getDefaultThemeDef() {
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
} // namespace fx
  //   return -1; // Use terminal default
  // }

// if (color_