#pragma once

#include <map>
#include <string>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

namespace flux {

// This defines WHAT colors a theme has (as hex strings or names)
// Used for loading from YAML files in standalone mode

struct ThemeDefinition {
  std::string name;

  // Color values as hex strings (e.g., "#FF0000")
  std::string background;
  std::string foreground;
  std::string selected;
  std::string directory;
  std::string executable;
  std::string hidden;
  std::string symlink;
  std::string parent_dir;
  std::string status_bar_bg;
  std::string status_bar_fg;
  std::string status_bar_active;
  std::string ui_secondary;
  std::string ui_border;
  std::string ui_error;
  std::string ui_warning;
  std::string ui_accent;
  std::string ui_info;
  std::string ui_success;
};

// ============================================
// Level 2: Applied Theme (COLOR_PAIR numbers)
// ============================================
// This is what the Renderer actually uses
// These are ncurses COLOR_PAIR numbers, ready to use

struct Theme {
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

// ============================================
// Theme Manager - Handles Color Initialization
// ============================================
// This class manages the conversion from ThemeDefinition to Theme

class ThemeManager {
public:
  ThemeManager();

  // FOR STANDALONE USE:
  // Creates ncurses color pairs from a theme definition
  // Returns a Theme with the COLOR_PAIR numbers
  Theme applyThemeDefinition(const ThemeDefinition &def);

  // FOR EMBEDDED USE (like Arc):
  // Just create a Theme struct directly with your existing COLOR_PAIRs
  // No initialization needed - you're using colors already set up
  // Example: Theme theme { .background = COLOR_PAIR(1), .foreground =
  // COLOR_PAIR(2), ... };

  // Load theme definition from TOML file
  static ThemeDefinition loadFromFile(const std::string &path);

  // Load theme definition from TOML string
  static ThemeDefinition loadFromString(const std::string &yaml);

  // Built-in theme definitions (as TOML)
  static ThemeDefinition getDefaultThemeDef();
  static ThemeDefinition getGruvboxThemeDef();
  static ThemeDefinition getNordThemeDef();
  static ThemeDefinition getDraculaThemeDef();

private:
  short next_color_id_;
  std::map<std::string, short> color_cache_;

  struct RGB {
    int r, g, b;
  };

  RGB parseHex(const std::string &hex) const;
  short createOrGetColor(const std::string &hex_color);
  short findClosestBasicColor(const RGB &rgb) const;
};

// ============================================
// Quick Start Functions
// ============================================

// For standalone use - creates both definition and applied theme
Theme loadThemeFromFile();
Theme createDefaultTheme();
Theme createGruvboxTheme();
Theme createNordTheme();
Theme createDraculaTheme();

} // namespace flux