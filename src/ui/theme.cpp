#include <cctype>
#include <flux/ui/theme.h>
#include <iostream>

namespace flux {

ThemeManager::ThemeManager() : next_color_id_(16) {
  // Start after the 16 basic colors
}

ThemeManager::RGB ThemeManager::parseHex(const std::string &hex) const {
  RGB rgb{0, 0, 0};

  if (hex.length() != 7 || hex[0] != '#') {
    return rgb;
  }

  try {
    rgb.r = std::stoi(hex.substr(1, 2), nullptr, 16);
    rgb.g = std::stoi(hex.substr(3, 2), nullptr, 16);
    rgb.b = std::stoi(hex.substr(5, 2), nullptr, 16);
  } catch (...) {
    // Return black on error
  }

  return rgb;
}

short ThemeManager::findClosestBasicColor(const RGB &rgb) const {
  // Map to closest basic 8 colors
  struct ColorMapping {
    short ncurses_color;
    RGB rgb;
  };

  static const ColorMapping basic_colors[] = {
      {COLOR_BLACK, {0, 0, 0}},    {COLOR_RED, {128, 0, 0}},
      {COLOR_GREEN, {0, 128, 0}},  {COLOR_YELLOW, {128, 128, 0}},
      {COLOR_BLUE, {0, 0, 128}},   {COLOR_MAGENTA, {128, 0, 128}},
      {COLOR_CYAN, {0, 128, 128}}, {COLOR_WHITE, {192, 192, 192}}};

  double min_distance = 1000000;
  short closest = COLOR_WHITE;

  for (const auto &color : basic_colors) {
    double dr = rgb.r - color.rgb.r;
    double dg = rgb.g - color.rgb.g;
    double db = rgb.b - color.rgb.b;
    double distance = dr * dr + dg * dg + db * db;

    if (distance < min_distance) {
      min_distance = distance;
      closest = color.ncurses_color;
    }
  }

  return closest;
}

short ThemeManager::createOrGetColor(const std::string &hex_color) {
  // Check cache first
  auto it = color_cache_.find(hex_color);
  if (it != color_cache_.end()) {
    return it->second;
  }

  // Handle special values
  if (hex_color == "transparent" || hex_color == "default" ||
      hex_color.empty()) {
    return -1; // Use terminal default
  }

  RGB rgb = parseHex(hex_color);

  // Try to create custom color if terminal supports it
  if (COLORS >= 256 && can_change_color() && next_color_id_ < COLORS) {
    short color_id = next_color_id_++;
    short r_1000 = (rgb.r * 1000) / 255;
    short g_1000 = (rgb.g * 1000) / 255;
    short b_1000 = (rgb.b * 1000) / 255;

    if (init_color(color_id, r_1000, g_1000, b_1000) == OK) {
      color_cache_[hex_color] = color_id;
      return color_id;
    }
  }

  // Fallback to basic colors
  short basic = findClosestBasicColor(rgb);
  color_cache_[hex_color] = basic;
  return basic;
}

Theme ThemeManager::applyThemeDefinition(const ThemeDefinition &def) {
  Theme theme;

  // Convert each color from hex to ncurses color
  short bg = createOrGetColor(def.background);
  short fg = createOrGetColor(def.foreground);
  short sel = createOrGetColor(def.selected);
  short dir = createOrGetColor(def.directory);
  short exe = createOrGetColor(def.executable);
  short hid = createOrGetColor(def.hidden);
  short sym = createOrGetColor(def.symlink);
  short par = createOrGetColor(def.parent_dir);
  short sb_bg = createOrGetColor(def.status_bar_bg);
  short sb_fg = createOrGetColor(def.status_bar_fg);
  short sb_act = createOrGetColor(def.status_bar_active);
  short ui_sec = createOrGetColor(def.ui_secondary);
  short ui_bor = createOrGetColor(def.ui_border);
  short ui_err = createOrGetColor(def.ui_error);
  short ui_war = createOrGetColor(def.ui_warning);
  short ui_acc = createOrGetColor(def.ui_accent);
  short ui_inf = createOrGetColor(def.ui_info);
  short ui_suc = createOrGetColor(def.ui_success);

  // Initialize color pairs
  // We start from pair 20 to avoid conflicts
  int pair_base = 20;

  // Set the default background/foreground
  assume_default_colors(fg, bg);

  init_pair(pair_base + 0, fg, bg);        // background/foreground
  init_pair(pair_base + 1, fg, sel);       // selected
  init_pair(pair_base + 2, dir, bg);       // directory
  init_pair(pair_base + 3, exe, bg);       // executable
  init_pair(pair_base + 4, hid, bg);       // hidden
  init_pair(pair_base + 5, sym, bg);       // symlink
  init_pair(pair_base + 6, par, bg);       // parent_dir
  init_pair(pair_base + 7, sb_fg, sb_bg);  // status_bar
  init_pair(pair_base + 8, sb_act, sb_bg); // status_bar_active
  init_pair(pair_base + 9, ui_sec, bg);    // ui_secondary
  init_pair(pair_base + 10, ui_bor, bg);   // ui_border
  init_pair(pair_base + 11, ui_err, bg);   // ui_error
  init_pair(pair_base + 12, ui_war, bg);   // ui_warning
  init_pair(pair_base + 13, ui_acc, bg);   // ui_accent
  init_pair(pair_base + 14, ui_inf, bg);   // ui_info
  init_pair(pair_base + 15, ui_suc, bg);   // ui_success

  // Fill theme with COLOR_PAIR numbers
  theme.background = pair_base + 0;
  theme.foreground = pair_base + 0;
  theme.selected = pair_base + 1;
  theme.directory = pair_base + 2;
  theme.executable = pair_base + 3;
  theme.hidden = pair_base + 4;
  theme.symlink = pair_base + 5;
  theme.parent_dir = pair_base + 6;
  theme.status_bar = pair_base + 7;
  theme.status_bar_active = pair_base + 8;
  theme.ui_secondary = pair_base + 9;
  theme.ui_border = pair_base + 10;
  theme.ui_error = pair_base + 11;
  theme.ui_warning = pair_base + 12;
  theme.ui_accent = pair_base + 13;
  theme.ui_info = pair_base + 14;
  theme.ui_success = pair_base + 15;

  return theme;
}

ThemeDefinition ThemeManager::getDefaultThemeDef() {
  return {
      "Default Dark",
      "transparent", // background
      "#C9D1D9",     // foreground
      "#264F78",     // selected
      "#79C0FF",     // directory
      "#7EE787",     // executable
      "#6E7681",     // hidden
      "#D2A8FF",     // symlink
      "#D2A8FF",     // parent_dir
      "#21262D",     // status_bar_bg
      "#C9D1D9",     // status_bar_fg
      "#58A6FF",     // status_bar_active
      "#8B949E",     // ui_secondary
      "#30363D",     // ui_border
      "#FF7B72",     // ui_error
      "#E3B341",     // ui_warning
      "#D2A8FF",     // ui_accent
      "#79C0FF",     // ui_info
      "#7EE787"      // ui_success
  };
}

ThemeDefinition ThemeManager::getGruvboxThemeDef() {
  return {
      "Gruvbox Dark",
      "#282828", // background
      "#ebdbb2", // foreground
      "#504945", // selected
      "#83a598", // directory
      "#b8bb26", // executable
      "#928374", // hidden
      "#8ec07c", // symlink
      "#d3869b", // parent_dir
      "#3c3836", // status_bar_bg
      "#ebdbb2", // status_bar_fg
      "#fe8019", // status_bar_active
      "#a89984", // ui_secondary
      "#504945", // ui_border
      "#fb4934", // ui_error
      "#fabd2f", // ui_warning
      "#d3869b", // ui_accent
      "#83a598", // ui_info
      "#b8bb26"  // ui_success
  };
}

ThemeDefinition ThemeManager::getNordThemeDef() {
  return {
      "Nord",
      "#2E3440", // background
      "#ECEFF4", // foreground
      "#434C5E", // selected
      "#88C0D0", // directory
      "#A3BE8C", // executable
      "#4C566A", // hidden
      "#8FBCBB", // symlink
      "#B48EAD", // parent_dir
      "#3B4252", // status_bar_bg
      "#ECEFF4", // status_bar_fg
      "#81A1C1", // status_bar_active
      "#D8DEE9", // ui_secondary
      "#434C5E", // ui_border
      "#BF616A", // ui_error
      "#EBCB8B", // ui_warning
      "#B48EAD", // ui_accent
      "#88C0D0", // ui_info
      "#A3BE8C"  // ui_success
  };
}

ThemeDefinition ThemeManager::getDraculaThemeDef() {
  return {
      "Dracula",
      "#282a36", // background
      "#f8f8f2", // foreground
      "#44475a", // selected
      "#bd93f9", // directory
      "#50fa7b", // executable
      "#6272a4", // hidden
      "#8be9fd", // symlink
      "#ff79c6", // parent_dir
      "#21222c", // status_bar_bg
      "#f8f8f2", // status_bar_fg
      "#ff79c6", // status_bar_active
      "#6272a4", // ui_secondary
      "#44475a", // ui_border
      "#ff5555", // ui_error
      "#f1fa8c", // ui_warning
      "#ff79c6", // ui_accent
      "#8be9fd", // ui_info
      "#50fa7b"  // ui_success
  };
}

// Quick start functions for standalone use
Theme createDefaultTheme() {
  ThemeManager manager;
  return manager.applyThemeDefinition(ThemeManager::getDefaultThemeDef());
}

Theme createGruvboxTheme() {
  ThemeManager manager;
  return manager.applyThemeDefinition(ThemeManager::getGruvboxThemeDef());
}

Theme createNordTheme() {
  ThemeManager manager;
  return manager.applyThemeDefinition(ThemeManager::getNordThemeDef());
}

Theme createDraculaTheme() {
  ThemeManager manager;
  return manager.applyThemeDefinition(ThemeManager::getDraculaThemeDef());
}

} // namespace flux