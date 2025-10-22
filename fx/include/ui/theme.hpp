// flux/ui/theme.h
#pragma once

#include <cstdint>
#include <string>

namespace fx {

struct Theme {
  uint32_t background;
  uint32_t foreground;
  uint32_t selected;
  uint32_t directory;
  uint32_t executable;
  uint32_t hidden;
  uint32_t symlink;
  uint32_t parent_dir;
  uint32_t status_bar;
  uint32_t status_bar_active;
  uint32_t ui_secondary;
  uint32_t ui_border;
  uint32_t ui_error;
  uint32_t ui_warning;
  uint32_t ui_accent;
  uint32_t ui_info;
  uint32_t ui_success;

  bool use_default_bg = false;
};

// Theme definition - string-based color specifications
// Used for loading themes from config files (TOML, YAML, etc.)
struct ThemeDefinition {
  std::string name;

  // Core colors
  std::string background;
  std::string foreground;

  // Specific element colors (used by standalone mode)
  std::string selected;
  std::string directory;
  std::string executable;
  std::string hidden;
  std::string symlink;
  std::string parent_dir;

  // State colors (for Arc compatibility)
  std::string state_active;
  std::string state_selected;
  std::string state_hover;
  std::string state_disabled;

  // Semantic UI colors
  std::string ui_primary;
  std::string ui_secondary;
  std::string ui_accent;
  std::string ui_success;
  std::string ui_warning;
  std::string ui_error;
  std::string ui_info;
  std::string ui_border;

  // Status bar
  std::string status_bar_bg;
  std::string status_bar_fg;
  std::string status_bar_active;
};

// Theme manager - handles color pair initialization
class ThemeManager {
public:
  ThemeManager();

  // Convert a ThemeDefinition to a Theme (only in standalone mode)
  Theme applyThemeDefinition(const ThemeDefinition &def);

  // Get the default theme definition
  static ThemeDefinition getDefaultThemeDef();

private:
  uint32_t parseHexColor(const std::string &color_str);
};

// Create a basic default theme (fallback)
Theme createDefaultTheme();

} // namespace fx