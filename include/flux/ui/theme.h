// flux/ui/theme.h
#pragma once

#include <string>

namespace flux
{

// Theme structure - contains COLOR_PAIR indices
// In embedded mode: these are indices into the host app's color table
// In standalone mode: these are Flux's own color pair indices (100+)
struct Theme
{
  int background;
  int foreground;
  int selected;
  int directory;
  int executable;
  int hidden;
  int symlink;
  int parent_dir;
  int status_bar;
  int status_bar_active;
  int ui_secondary;
  int ui_border;
  int ui_error;
  int ui_warning;
  int ui_accent;
  int ui_info;
  int ui_success;
};

// Theme definition - string-based color specifications
// Used for loading themes from config files (TOML, YAML, etc.)
struct ThemeDefinition
{
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
class ThemeManager
{
public:
  ThemeManager();

  // Set embedded mode - when true, applyThemeDefinition won't call init_pair()
  void setEmbeddedMode(bool embedded);

  // Convert a ThemeDefinition to a Theme (only in standalone mode)
  Theme applyThemeDefinition(const ThemeDefinition &def);

  // Get the default theme definition
  static ThemeDefinition getDefaultThemeDef();

private:
  bool embedded_mode_;

  // Parse a color string to an ncurses color constant
  short parseColor(const std::string &color_str);
};

// Create a basic default theme (fallback)
Theme createDefaultTheme();

} // namespace flux