#include "flux/ui/renderer.h"
#include "flux/ui/theme.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace flux {

Renderer::Renderer()
    : height_(0), width_(0), viewport_height_(0),
      theme_(createDefaultTheme()), // Use default theme initially
      icon_provider_(IconStyle::AUTO) {}

void Renderer::setTheme(const Theme &theme) { theme_ = theme; }

void Renderer::setIconStyle(IconStyle style) {
  icon_provider_ = IconProvider(style);
}

void Renderer::render(const Browser &browser) {
  getmaxyx(stdscr, height_, width_);
  viewport_height_ = std::max(1, height_ - 4);

  erase();
  renderHeader(browser);
  renderFileList(browser);
  renderStatus(browser);
  refresh();
}

void Renderer::renderHeader(const Browser &browser) {
  // Top bar with title
  attron(COLOR_PAIR(theme_.status_bar));
  mvprintw(0, 0, " ");

  attron(A_BOLD);
  printw("File Browser");
  attroff(A_BOLD);

  // File counts in secondary color
  size_t dirs = browser.getDirectoryCount();
  size_t files = browser.getFileCount();
  printw("  ");
  attron(COLOR_PAIR(theme_.ui_secondary));
  printw("%zu dirs, %zu files", dirs, files);
  attroff(COLOR_PAIR(theme_.ui_secondary));

  attron(COLOR_PAIR(theme_.status_bar));
  clrtoeol();
  attroff(COLOR_PAIR(theme_.status_bar));

  // Current path with icon
  move(1, 0);
  attron(COLOR_PAIR(theme_.ui_border));

  std::string path = browser.getCurrentPath().string();
  int max_path_len = width_ - 6;

  if (path.length() > static_cast<size_t>(max_path_len)) {
    path = "..." + path.substr(path.length() - (max_path_len - 3));
  }

  printw("  %s", path.c_str());
  clrtoeol();
  attroff(COLOR_PAIR(theme_.ui_border));
}

void Renderer::renderFileList(const Browser &browser) {
  const auto &entries = browser.getEntries();
  size_t selected = browser.getSelectedIndex();
  size_t scroll = browser.getScrollOffset();

  int start_y = 2;
  size_t visible_end = std::min(scroll + viewport_height_, entries.size());

  for (size_t i = scroll; i < visible_end; ++i) {
    int y = start_y + (i - scroll);
    const auto &entry = entries[i];
    bool is_selected = (i == selected);

    move(y, 0);

    // Determine colors and icon based on file type
    int name_color = theme_.foreground;
    bool use_bold = false;
    std::string icon;

    if (entry.is_directory) {
      if (entry.name == "..") {
        icon = icon_provider_.getParentIcon();
        name_color = theme_.parent_dir;
        use_bold = true;
      } else {
        icon = icon_provider_.getDirectoryIcon();
        name_color = theme_.directory;
        use_bold = true;
      }
    } else if (entry.is_executable) {
      icon = icon_provider_.getExecutableIcon();
      name_color = theme_.executable;
      use_bold = true;
    } else if (entry.is_symlink) {
      icon = icon_provider_.getSymlinkIcon();
      name_color = theme_.symlink;
    } else if (entry.is_hidden) {
      icon = icon_provider_.getHiddenIcon();
      name_color = theme_.hidden;
    } else {
      // Use file extension-based icon
      icon = icon_provider_.getFileIcon(entry.name);
    }

    // Selection styling
    if (is_selected) {
      attron(COLOR_PAIR(theme_.selected));
      printw(" %s ", icon.c_str());

      if (use_bold) {
        attron(COLOR_PAIR(name_color) | A_BOLD | A_REVERSE);
      } else {
        attron(COLOR_PAIR(name_color) | A_REVERSE);
      }
    } else {
      attron(COLOR_PAIR(theme_.ui_secondary));
      printw(" %s ", icon.c_str());
      attroff(COLOR_PAIR(theme_.ui_secondary));

      if (use_bold) {
        attron(COLOR_PAIR(name_color) | A_BOLD);
      } else {
        attron(COLOR_PAIR(name_color));
      }
    }

    // Calculate max name width
    int max_name_width = width_ - 18;
    std::string display_name = entry.name;

    if (display_name.length() > static_cast<size_t>(max_name_width)) {
      display_name = display_name.substr(0, max_name_width - 3) + "...";
    }

    printw("%s", display_name.c_str());

    // Calculate position for size (right-aligned)
    int current_x = getcurx(stdscr);
    int size_width = 11;
    int target_x = width_ - size_width;

    // Fill space between name and size
    if (is_selected) {
      for (int p = current_x; p < target_x; ++p) {
        addch(' ');
      }
    } else {
      if (use_bold) {
        attroff(COLOR_PAIR(name_color) | A_BOLD);
      } else {
        attroff(COLOR_PAIR(name_color));
      }

      for (int p = current_x; p < target_x; ++p) {
        addch(' ');
      }
    }

    // Print size info
    if (is_selected) {
      attroff(COLOR_PAIR(name_color) | A_REVERSE | A_BOLD);
      attron(COLOR_PAIR(theme_.selected));
    }

    attron(COLOR_PAIR(theme_.ui_secondary));

    if (!entry.is_directory && entry.name != "..") {
      printw("%10s", formatSize(entry.size).c_str());
    } else if (entry.is_directory && entry.name != "..") {
      printw("     <DIR>");
    } else {
      printw("          ");
    }

    attroff(COLOR_PAIR(theme_.ui_secondary));

    if (is_selected) {
      addch(' ');
      attroff(COLOR_PAIR(theme_.selected));
    }

    clrtoeol();
  }

  // Clear remaining viewport
  for (int y = start_y + (visible_end - scroll); y < height_ - 2; ++y) {
    move(y, 0);
    clrtoeol();
  }
}

void Renderer::renderStatus(const Browser &browser) {
  int status_y = height_ - 2;

  // Subtle separator
  attron(COLOR_PAIR(theme_.ui_border));
  move(status_y - 1, 0);
  for (int i = 0; i < width_; ++i) {
    addch(ACS_HLINE);
  }
  attroff(COLOR_PAIR(theme_.ui_border));

  // Status line
  if (browser.hasError()) {
    move(status_y, 0);
    attron(COLOR_PAIR(theme_.ui_error));
    printw(" ! ");
    attron(A_BOLD);
    printw("%s", browser.getErrorMessage().c_str());
    attroff(A_BOLD);
    clrtoeol();
    attroff(COLOR_PAIR(theme_.ui_error));
  } else {
    move(status_y, 0);
    attron(COLOR_PAIR(theme_.status_bar));

    size_t selected = browser.getSelectedIndex() + 1;
    size_t total = browser.getTotalEntries();

    // Position indicator
    printw(" ");
    attron(COLOR_PAIR(theme_.status_bar_active) | A_BOLD);
    printw("%zu", selected);
    attroff(COLOR_PAIR(theme_.status_bar_active) | A_BOLD);
    attron(COLOR_PAIR(theme_.status_bar));
    printw("/");
    attron(COLOR_PAIR(theme_.ui_secondary));
    printw("%zu", total);
    attroff(COLOR_PAIR(theme_.ui_secondary));
    attron(COLOR_PAIR(theme_.status_bar));

    // Flags
    if (browser.getShowHidden()) {
      printw("  ");
      attron(COLOR_PAIR(theme_.ui_warning));
      printw("[hidden]");
      attroff(COLOR_PAIR(theme_.ui_warning));
      attron(COLOR_PAIR(theme_.status_bar));
    }

    // Sort mode
    printw("  ");
    attron(COLOR_PAIR(theme_.ui_secondary));
    printw("sort:");
    attroff(COLOR_PAIR(theme_.ui_secondary));
    attron(COLOR_PAIR(theme_.status_bar));
    printw(" ");
    attron(COLOR_PAIR(theme_.status_bar_active));
    switch (browser.getSortMode()) {
    case Browser::SortMode::NAME:
      printw("name");
      break;
    case Browser::SortMode::SIZE:
      printw("size");
      break;
    case Browser::SortMode::DATE:
      printw("date");
      break;
    case Browser::SortMode::TYPE:
      printw("type");
      break;
    }
    attroff(COLOR_PAIR(theme_.status_bar_active));

    clrtoeol();
    attroff(COLOR_PAIR(theme_.status_bar));
  }

  // Help line
  status_y++;
  move(status_y, 0);

  attron(COLOR_PAIR(theme_.ui_secondary));

  // Group related commands
  printw(" ");
  attron(A_BOLD);
  printw("j");
  attroff(A_BOLD);
  printw("/");
  attron(A_BOLD);
  printw("k");
  attroff(A_BOLD);
  printw(" move");

  printw("  ");
  attron(A_BOLD);
  printw("Enter");
  attroff(A_BOLD);
  printw(" open");

  printw("  ");
  attron(A_BOLD);
  printw("h");
  attroff(A_BOLD);
  printw(" back");

  printw("  ");
  attron(A_BOLD);
  printw(".");
  attroff(A_BOLD);
  printw(" hidden");

  printw("  ");
  attron(A_BOLD);
  printw("s");
  attroff(A_BOLD);
  printw(" sort");

  printw("  ");
  attron(A_BOLD);
  printw("q");
  attroff(A_BOLD);
  printw(" quit");

  clrtoeol();
  attroff(COLOR_PAIR(theme_.ui_secondary));
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

} // namespace flux