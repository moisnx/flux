#include "include/ui/renderer.hpp"
#include "include/ui/theme.hpp"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <notcurses/notcurses.h>
#include <sstream>

using namespace flux;
namespace fx {

Renderer::Renderer(notcurses *nc, ncplane *stdplane)
    : nc_(nc), stdplane_(stdplane), viewport_height_(0),
      theme_(createDefaultTheme()), icon_provider_(IconStyle::AUTO) {}

void Renderer::setTheme(const Theme &theme) {
  theme_ = theme;

  // CRITICAL FIX: Set the base background when theme changes
  uint64_t channels = 0;
  ncchannels_set_fg_rgb8(&channels, (theme_.foreground >> 16) & 0xFF,
                         (theme_.foreground >> 8) & 0xFF,
                         theme_.foreground & 0xFF);
  ncchannels_set_bg_rgb8(&channels, (theme_.background >> 16) & 0xFF,
                         (theme_.background >> 8) & 0xFF,
                         theme_.background & 0xFF);
  ncplane_set_base(stdplane_, " ", 0, channels);
}

void Renderer::setIconStyle(IconStyle style) {
  icon_provider_ = IconProvider(style);
}

void Renderer::setColors(uint32_t fg_rgb) {
  uint64_t channels = 0;
  ncchannels_set_fg_rgb8(&channels, (fg_rgb >> 16) & 0xFF, (fg_rgb >> 8) & 0xFF,
                         fg_rgb & 0xFF);
  ncchannels_set_bg_default(&channels);
  ncplane_set_channels(stdplane_, channels);
}

void Renderer::setColors(uint32_t fg_rgb, uint32_t bg_rgb) {
  uint64_t channels = 0;
  ncchannels_set_fg_rgb8(&channels,
                         (fg_rgb >> 16) & 0xFF, // R
                         (fg_rgb >> 8) & 0xFF,  // G
                         fg_rgb & 0xFF);        // B
  ncchannels_set_bg_rgb8(&channels,
                         (bg_rgb >> 16) & 0xFF, // R
                         (bg_rgb >> 8) & 0xFF,  // G
                         bg_rgb & 0xFF);        // B
  ncplane_set_channels(stdplane_, channels);
}

void Renderer::clearToEOL() {
  unsigned width, height;
  ncplane_dim_yx(stdplane_, &height, &width);

  unsigned cur_x, cur_y;
  ncplane_cursor_yx(stdplane_, &cur_y, &cur_x);

  if (cur_x < width) {
    std::string spaces(width - cur_x, ' ');
    ncplane_putstr(stdplane_, spaces.c_str());
  }
}

void Renderer::render(const Browser &browser) {
  unsigned width, height;
  ncplane_dim_yx(stdplane_, &height, &width);
  viewport_height_ = std::max(1, static_cast<int>(height) - 4);

  // CRITICAL FIX: Erase with proper background color
  // First set the channels for erasing
  uint64_t erase_channels = 0;
  ncchannels_set_fg_rgb8(&erase_channels, (theme_.foreground >> 16) & 0xFF,
                         (theme_.foreground >> 8) & 0xFF,
                         theme_.foreground & 0xFF);
  ncchannels_set_bg_rgb8(&erase_channels, (theme_.background >> 16) & 0xFF,
                         (theme_.background >> 8) & 0xFF,
                         theme_.background & 0xFF);
  ncplane_set_channels(stdplane_, erase_channels);

  ncplane_set_styles(stdplane_, NCSTYLE_NONE);
  ncplane_erase(stdplane_);

  renderHeader(browser);
  renderFileList(browser);
  renderStatus(browser);
  notcurses_render(nc_);
}

void Renderer::renderHeader(const Browser &browser) {
  unsigned width, height;
  ncplane_dim_yx(stdplane_, &height, &width);

  // Status bar
  setColors(theme_.foreground, theme_.status_bar);
  ncplane_cursor_move_yx(stdplane_, 0, 0);
  ncplane_putstr(stdplane_, " ");

  ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
  ncplane_putstr(stdplane_, "File Browser");
  ncplane_set_styles(stdplane_, NCSTYLE_NONE);

  size_t dirs = browser.getDirectoryCount();
  size_t files = browser.getFileCount();
  ncplane_putstr(stdplane_, "  ");

  // Secondary text on status bar
  setColors(theme_.ui_secondary, theme_.status_bar);
  ncplane_printf(stdplane_, "%zu dirs, %zu files", dirs, files);

  setColors(theme_.foreground, theme_.status_bar);
  clearToEOL();

  // Path line - use background color here
  ncplane_cursor_move_yx(stdplane_, 1, 0);
  setColors(theme_.ui_border, theme_.background);

  std::string path = browser.getCurrentPath().string();
  int max_path_len = width - 6;

  if (path.length() > static_cast<size_t>(max_path_len)) {
    path = "..." + path.substr(path.length() - (max_path_len - 3));
  }

  ncplane_printf(stdplane_, "  %s", path.c_str());
  clearToEOL();
}

void Renderer::renderFileList(const Browser &browser) {
  unsigned width, height;
  ncplane_dim_yx(stdplane_, &height, &width);

  const auto &entries = browser.getEntries();
  size_t selected = browser.getSelectedIndex();
  size_t scroll = browser.getScrollOffset();

  int start_y = 2;
  size_t visible_end = std::min(scroll + viewport_height_, entries.size());

  for (size_t i = scroll; i < visible_end; ++i) {
    int y = start_y + (i - scroll);
    const auto &entry = entries[i];
    bool is_selected = (i == selected);

    ncplane_cursor_move_yx(stdplane_, y, 0);

    // Determine colors and icon based on file type
    uint32_t name_color_rgb = theme_.foreground;
    bool use_bold = false;
    std::string icon;

    if (entry.is_directory) {
      if (entry.name == "..") {
        icon = icon_provider_.getParentIcon();
        name_color_rgb = theme_.parent_dir;
        use_bold = true;
      } else {
        icon = icon_provider_.getDirectoryIcon();
        name_color_rgb = theme_.directory;
        use_bold = true;
      }
    } else if (entry.is_executable) {
      icon = icon_provider_.getExecutableIcon();
      name_color_rgb = theme_.executable;
      use_bold = true;
    } else if (entry.is_symlink) {
      icon = icon_provider_.getSymlinkIcon();
      name_color_rgb = theme_.symlink;
    } else if (entry.is_hidden) {
      icon = icon_provider_.getHiddenIcon();
      name_color_rgb = theme_.hidden;
    } else {
      icon = icon_provider_.getFileIcon(entry.name);
      name_color_rgb = theme_.foreground;
    }

    // Selection styling - use theme background when not selected
    if (is_selected) {
      // Selected row: use selection background for entire line
      setColors(theme_.foreground, theme_.selected);
      ncplane_printf(stdplane_, " %s ", icon.c_str());

      // Name with semantic color on selection background
      setColors(name_color_rgb, theme_.selected);
      if (use_bold) {
        ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
      }
    } else {
      // Non-selected: icon in secondary color with theme background
      setColors(theme_.ui_secondary, theme_.background);
      ncplane_printf(stdplane_, " %s ", icon.c_str());

      // Name in semantic color with theme background
      setColors(name_color_rgb, theme_.background);
      if (use_bold) {
        ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
      }
    }

    // Print filename
    int max_name_width = width - 18;
    std::string display_name = entry.name;

    if (display_name.length() > static_cast<size_t>(max_name_width)) {
      display_name = display_name.substr(0, max_name_width - 3) + "...";
    }

    ncplane_putstr(stdplane_, display_name.c_str());

    // Clear all styles after name
    ncplane_set_styles(stdplane_, NCSTYLE_NONE);

    // Get current position
    unsigned current_x, current_y;
    ncplane_cursor_yx(stdplane_, &current_y, &current_x);
    int size_width = 11;
    int target_x = width - size_width;

    // Fill gap with appropriate background
    if (is_selected) {
      // Keep selection background across gap
      setColors(theme_.foreground, theme_.selected);
      for (int p = current_x; p < target_x; ++p) {
        ncplane_putchar(stdplane_, ' ');
      }

      // Size info on selection background
      setColors(theme_.ui_secondary, theme_.selected);
    } else {
      // Theme background for gap
      setColors(theme_.foreground, theme_.background);
      for (int p = current_x; p < target_x; ++p) {
        ncplane_putchar(stdplane_, ' ');
      }

      // Size info in secondary color with theme background
      setColors(theme_.ui_secondary, theme_.background);
    }

    // Print size information
    if (!entry.is_directory && entry.name != "..") {
      ncplane_printf(stdplane_, "%10s", formatSize(entry.size).c_str());
    } else if (entry.is_directory && entry.name != "..") {
      ncplane_putstr(stdplane_, "     <DIR>");
    } else {
      ncplane_putstr(stdplane_, "          ");
    }

    // Final padding for selected items
    if (is_selected) {
      setColors(theme_.foreground, theme_.selected);
      ncplane_putchar(stdplane_, ' ');
    }

    // Clear to end of line with correct background
    if (is_selected) {
      setColors(theme_.foreground, theme_.selected);
    } else {
      setColors(theme_.foreground, theme_.background);
    }
    clearToEOL();

    // CRITICAL: Reset all styles at end of line
    ncplane_set_styles(stdplane_, NCSTYLE_NONE);
  }

  // Clear remaining viewport with theme background
  setColors(theme_.foreground, theme_.background);
  ncplane_set_styles(stdplane_, NCSTYLE_NONE);
  for (int y = start_y + (visible_end - scroll);
       y < static_cast<int>(height) - 2; ++y) {
    ncplane_cursor_move_yx(stdplane_, y, 0);
    clearToEOL();
  }
}

void Renderer::renderStatus(const Browser &browser) {
  unsigned width, height;
  ncplane_dim_yx(stdplane_, &height, &width);
  int status_y = height - 2;

  // Separator line
  setColors(theme_.ui_border, theme_.background);
  ncplane_cursor_move_yx(stdplane_, status_y - 1, 0);
  for (unsigned i = 0; i < width; ++i) {
    ncplane_putstr(stdplane_, "─");
  }

  // Status line
  ncplane_cursor_move_yx(stdplane_, status_y, 0);

  if (browser.hasError()) {
    setColors(theme_.ui_error, theme_.background);
    ncplane_putstr(stdplane_, " ! ");
    ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
    ncplane_putstr(stdplane_, browser.getErrorMessage().c_str());
    ncplane_set_styles(stdplane_, NCSTYLE_NONE);
    clearToEOL();
  } else {
    size_t selected = browser.getSelectedIndex() + 1;
    size_t total = browser.getTotalEntries();

    setColors(theme_.foreground, theme_.status_bar);
    ncplane_putstr(stdplane_, " ");

    setColors(theme_.status_bar_active, theme_.status_bar);
    ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
    ncplane_printf(stdplane_, "%zu", selected);
    ncplane_set_styles(stdplane_, NCSTYLE_NONE);

    setColors(theme_.foreground, theme_.status_bar);
    ncplane_putstr(stdplane_, "/");

    setColors(theme_.ui_secondary, theme_.status_bar);
    ncplane_printf(stdplane_, "%zu", total);

    if (browser.getShowHidden()) {
      setColors(theme_.foreground, theme_.status_bar);
      ncplane_putstr(stdplane_, "  ");
      setColors(theme_.ui_warning, theme_.status_bar);
      ncplane_putstr(stdplane_, "[hidden]");
    }

    setColors(theme_.foreground, theme_.status_bar);
    ncplane_putstr(stdplane_, "  ");
    setColors(theme_.ui_secondary, theme_.status_bar);
    ncplane_putstr(stdplane_, "sort: ");

    setColors(theme_.status_bar_active, theme_.status_bar);
    switch (browser.getSortMode()) {
    case Browser::SortMode::NAME:
      ncplane_putstr(stdplane_, "name");
      break;
    case Browser::SortMode::SIZE:
      ncplane_putstr(stdplane_, "size");
      break;
    case Browser::SortMode::DATE:
      ncplane_putstr(stdplane_, "date");
      break;
    case Browser::SortMode::TYPE:
      ncplane_putstr(stdplane_, "type");
      break;
    }

    setColors(theme_.foreground, theme_.status_bar);
    clearToEOL();
  }

  // Help line
  status_y++;
  ncplane_cursor_move_yx(stdplane_, status_y, 0);
  setColors(theme_.ui_secondary, theme_.background);

  ncplane_putstr(stdplane_, " ");
  ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
  ncplane_putstr(stdplane_, "j");
  ncplane_set_styles(stdplane_, NCSTYLE_NONE);
  ncplane_putstr(stdplane_, "/");
  ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
  ncplane_putstr(stdplane_, "k");
  ncplane_set_styles(stdplane_, NCSTYLE_NONE);
  ncplane_putstr(stdplane_, " move");

  ncplane_putstr(stdplane_, "  ");
  ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
  ncplane_putstr(stdplane_, "Enter");
  ncplane_set_styles(stdplane_, NCSTYLE_NONE);
  ncplane_putstr(stdplane_, " open");

  ncplane_putstr(stdplane_, "  ");
  ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
  ncplane_putstr(stdplane_, "h");
  ncplane_set_styles(stdplane_, NCSTYLE_NONE);
  ncplane_putstr(stdplane_, " back");

  ncplane_putstr(stdplane_, "  ");
  ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
  ncplane_putstr(stdplane_, ".");
  ncplane_set_styles(stdplane_, NCSTYLE_NONE);
  ncplane_putstr(stdplane_, " hidden");

  ncplane_putstr(stdplane_, "  ");
  ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
  ncplane_putstr(stdplane_, "s");
  ncplane_set_styles(stdplane_, NCSTYLE_NONE);
  ncplane_putstr(stdplane_, " sort");

  ncplane_putstr(stdplane_, "  ");
  ncplane_set_styles(stdplane_, NCSTYLE_BOLD);
  ncplane_putstr(stdplane_, "q");
  ncplane_set_styles(stdplane_, NCSTYLE_NONE);
  ncplane_putstr(stdplane_, " quit");

  clearToEOL();
}

std::string Renderer::formatSize(uintmax_t size) const {
  const char *units[] = {"B", "K", "M", "G", "T"};
  int unit = 0;
  double s = static_cast<double>(size);

  while (s >= 1024.0 && unit < 4) {
    s /= 1024.0;
    unit++;
  }

  std::ostringstream oss;
  if (unit == 0) {
    oss << size << units[unit];
  } else {
    oss << std::fixed << std::setprecision(1) << s << units[unit];
  }

  return oss.str();
}

} // namespace fx