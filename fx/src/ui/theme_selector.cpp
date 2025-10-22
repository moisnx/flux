#include "include/ui/theme_selector.hpp"
#include <algorithm>
#include <cstring>
#include <set>

namespace fx {

ThemeSelector::ThemeSelector(notcurses *nc, ncplane *stdplane)
    : nc_(nc), stdplane_(stdplane), selected_index_(0), scroll_offset_(0) {}

void ThemeSelector::loadAvailableThemes() {
  themes_.clear();
  auto search_paths = ThemeLoader::getThemeSearchPaths();

  std::set<std::string> seen_names;

  for (const auto &base_path : search_paths) {
    if (!std::filesystem::exists(base_path))
      continue;

    try {
      for (const auto &entry : std::filesystem::directory_iterator(base_path)) {
        if (!entry.is_regular_file())
          continue;

        auto path = entry.path();
        if (path.extension() != ".toml")
          continue;

        std::string theme_name = path.stem().string();

        // Skip duplicates (prioritize first found)
        if (seen_names.count(theme_name))
          continue;
        seen_names.insert(theme_name);

        try {
          ThemeDefinition def = ThemeLoader::loadFromTOML(path.string());
          themes_.push_back({theme_name, path, def});
        } catch (...) {
          // Skip invalid themes
        }
      }
    } catch (...) {
      continue;
    }
  }

  // Sort alphabetically
  std::sort(
      themes_.begin(), themes_.end(),
      [](const ThemeEntry &a, const ThemeEntry &b) { return a.name < b.name; });
}

uint64_t ThemeSelector::makeChannels(uint32_t fg, uint32_t bg) {
  uint64_t channels = 0;
  ncchannels_set_fg_rgb8(&channels, (fg >> 16) & 0xFF, (fg >> 8) & 0xFF,
                         fg & 0xFF);
  ncchannels_set_bg_rgb8(&channels, (bg >> 16) & 0xFF, (bg >> 8) & 0xFF,
                         bg & 0xFF);
  return channels;
}

void ThemeSelector::selectNext() {
  if (!themes_.empty() && selected_index_ < themes_.size() - 1) {
    selected_index_++;
  }
}

void ThemeSelector::selectPrevious() {
  if (selected_index_ > 0) {
    selected_index_--;
  }
}

int ThemeSelector::getViewportHeight() const {
  unsigned height, width;
  ncplane_dim_yx(stdplane_, &height, &width);
  return height;
}

void ThemeSelector::renderThemeList(ncplane *list_plane, int height) {
  ncplane_erase(list_plane);

  // Get current theme for styling
  const auto &current_def = themes_[selected_index_].definition;
  ThemeManager tm;
  Theme current_theme = tm.applyThemeDefinition(current_def);

  // Draw border
  uint64_t border_ch =
      makeChannels(current_theme.ui_border, current_theme.background);
  ncplane_set_channels(list_plane, border_ch);
  ncplane_putstr_yx(list_plane, 0, 0, "╭");
  ncplane_putstr(list_plane, "─ Themes ");
  for (int i = 10; i < 38; i++)
    ncplane_putstr(list_plane, "─");
  ncplane_putstr(list_plane, "╮");

  for (int i = 1; i < height - 1; i++) {
    ncplane_putstr_yx(list_plane, i, 0, "│");
    ncplane_putstr_yx(list_plane, i, 39, "│");
  }

  ncplane_putstr_yx(list_plane, height - 1, 0, "╰");
  for (int i = 1; i < 39; i++)
    ncplane_putstr(list_plane, "─");
  ncplane_putstr(list_plane, "╯");

  // Adjust scroll offset
  int visible_lines = height - 4;
  if (selected_index_ < scroll_offset_) {
    scroll_offset_ = selected_index_;
  } else if (selected_index_ >= scroll_offset_ + visible_lines) {
    scroll_offset_ = selected_index_ - visible_lines + 1;
  }

  // Render theme list
  for (size_t i = 0; i < themes_.size(); i++) {
    if (i < scroll_offset_)
      continue;
    if (i >= scroll_offset_ + visible_lines)
      break;

    int y = 2 + (i - scroll_offset_);

    if (i == selected_index_) {
      // Selected item
      uint64_t sel_ch =
          makeChannels(current_theme.foreground, current_theme.selected);
      ncplane_set_channels(list_plane, sel_ch);
      ncplane_putstr_yx(list_plane, y, 2, "► ");
    } else {
      // Normal item
      uint64_t norm_ch =
          makeChannels(current_theme.foreground, current_theme.background);
      ncplane_set_channels(list_plane, norm_ch);
      ncplane_putstr_yx(list_plane, y, 2, "  ");
    }

    std::string display_name = themes_[i].name;
    if (display_name.length() > 33) {
      display_name = display_name.substr(0, 30) + "...";
    }
    ncplane_putstr(list_plane, display_name.c_str());
  }

  // Scroll indicators
  if (scroll_offset_ > 0) {
    uint64_t indicator_ch =
        makeChannels(current_theme.ui_accent, current_theme.background);
    ncplane_set_channels(list_plane, indicator_ch);
    ncplane_putstr_yx(list_plane, 1, 19, "▲");
  }
  if (scroll_offset_ + visible_lines < themes_.size()) {
    uint64_t indicator_ch =
        makeChannels(current_theme.ui_accent, current_theme.background);
    ncplane_set_channels(list_plane, indicator_ch);
    ncplane_putstr_yx(list_plane, height - 2, 19, "▼");
  }
}

void ThemeSelector::renderPreview(ncplane *preview_plane, const Theme &theme) {
  ncplane_erase(preview_plane);
  unsigned height, width;
  ncplane_dim_yx(preview_plane, &height, &width);

  // Draw border
  uint64_t border_ch = makeChannels(theme.ui_border, theme.background);
  ncplane_set_channels(preview_plane, border_ch);
  ncplane_putstr_yx(preview_plane, 0, 0, "╭");
  ncplane_putstr(preview_plane, "─ Preview ");
  for (unsigned i = 11; i < width - 1; i++)
    ncplane_putstr(preview_plane, "─");
  ncplane_putstr(preview_plane, "╮");

  for (unsigned i = 1; i < height - 1; i++) {
    ncplane_putstr_yx(preview_plane, i, 0, "│");
    ncplane_putstr_yx(preview_plane, i, width - 1, "│");
  }

  ncplane_putstr_yx(preview_plane, height - 1, 0, "╰");
  for (unsigned i = 1; i < width - 1; i++)
    ncplane_putstr(preview_plane, "─");
  ncplane_putstr(preview_plane, "╯");

  // Preview content
  int y = 2;

  // Show colors
  auto draw_color = [&](const char *label, uint32_t color) {
    if (y >= (int)height - 2)
      return;
    uint64_t ch = makeChannels(color, theme.background);
    ncplane_set_channels(preview_plane, ch);
    ncplane_putstr_yx(preview_plane, y++, 3, label);
  };

  draw_color("█ Directory", theme.directory);
  draw_color("█ Executable", theme.executable);
  draw_color("█ Symlink", theme.symlink);
  draw_color("█ Hidden", theme.hidden);
  y++;

  if (y < (int)height - 2) {
    uint64_t selected_ch = makeChannels(theme.foreground, theme.selected);
    ncplane_set_channels(preview_plane, selected_ch);
    ncplane_putstr_yx(preview_plane, y++, 3, "  Selected Item  ");
  }

  y++;
  if (y < (int)height - 2) {
    uint64_t error_ch = makeChannels(theme.ui_error, theme.background);
    ncplane_set_channels(preview_plane, error_ch);
    ncplane_putstr_yx(preview_plane, y++, 3, "█ Error");
  }

  if (y < (int)height - 2) {
    uint64_t warn_ch = makeChannels(theme.ui_warning, theme.background);
    ncplane_set_channels(preview_plane, warn_ch);
    ncplane_putstr_yx(preview_plane, y++, 3, "█ Warning");
  }

  if (y < (int)height - 2) {
    uint64_t success_ch = makeChannels(theme.ui_success, theme.background);
    ncplane_set_channels(preview_plane, success_ch);
    ncplane_putstr_yx(preview_plane, y++, 3, "█ Success");
  }
}

void ThemeSelector::renderInstructions(ncplane *plane) {
  ncplane_erase(plane);
  unsigned height, width;
  ncplane_dim_yx(plane, &height, &width);

  const auto &current_def = themes_[selected_index_].definition;
  ThemeManager tm;
  Theme theme = tm.applyThemeDefinition(current_def);

  uint64_t text_ch = makeChannels(theme.ui_secondary, theme.background);
  ncplane_set_channels(plane, text_ch);

  const char *instructions = "↑/k: Up  ↓/j: Down  Enter: Select  q/Esc: Cancel";
  int x = (width - strlen(instructions)) / 2;
  ncplane_putstr_yx(plane, 0, x > 0 ? x : 0, instructions);
}

std::optional<ThemeEntry>
ThemeSelector::show(const std::string &current_theme) {
  loadAvailableThemes();

  if (themes_.empty()) {
    return std::nullopt;
  }

  // Find current theme index
  for (size_t i = 0; i < themes_.size(); i++) {
    if (themes_[i].name == current_theme) {
      selected_index_ = i;
      break;
    }
  }

  unsigned screen_height, screen_width;
  ncplane_dim_yx(stdplane_, &screen_height, &screen_width);

  // Responsive layout: adjust based on screen size
  bool compact_mode = screen_width < 80;
  unsigned list_width = compact_mode ? (screen_width > 50 ? 30 : 25) : 40;
  unsigned preview_x = list_width + 4;
  unsigned preview_width =
      screen_width > preview_x + 10 ? screen_width - preview_x - 2 : 10;

  // Create planes for layout
  struct ncplane_options list_opts = {.y = 2,
                                      .x = 2,
                                      .rows = screen_height - 5,
                                      .cols = list_width,
                                      .userptr = nullptr,
                                      .name = "theme_list",
                                      .resizecb = nullptr,
                                      .flags = 0,
                                      .margin_b = 0,
                                      .margin_r = 0};

  struct ncplane_options preview_opts = {.y = 2,
                                         .x = static_cast<int>(preview_x),
                                         .rows = screen_height - 5,
                                         .cols = preview_width,
                                         .userptr = nullptr,
                                         .name = "theme_preview",
                                         .resizecb = nullptr,
                                         .flags = 0,
                                         .margin_b = 0,
                                         .margin_r = 0};

  struct ncplane_options instr_opts = {.y = static_cast<int>(screen_height - 2),
                                       .x = 0,
                                       .rows = 1,
                                       .cols = screen_width,
                                       .userptr = nullptr,
                                       .name = "instructions",
                                       .resizecb = nullptr,
                                       .flags = 0,
                                       .margin_b = 0,
                                       .margin_r = 0};

  ncplane *list_plane = ncplane_create(stdplane_, &list_opts);
  ncplane *preview_plane = ncplane_create(stdplane_, &preview_opts);
  ncplane *instr_plane = ncplane_create(stdplane_, &instr_opts);

  bool running = true;
  std::optional<ThemeEntry> result = std::nullopt;

  while (running) {
    // Get current theme for preview
    ThemeManager tm;
    Theme preview_theme =
        tm.applyThemeDefinition(themes_[selected_index_].definition);

    // Apply background to main plane
    if (!preview_theme.use_default_bg) {
      uint64_t bg_ch =
          makeChannels(preview_theme.foreground, preview_theme.background);
      ncplane_set_base(stdplane_, " ", 0, bg_ch);
    }

    ncplane_erase(stdplane_);

    // Draw title
    uint64_t title_ch =
        makeChannels(preview_theme.ui_accent, preview_theme.background);
    ncplane_set_channels(stdplane_, title_ch);
    const char *title = "Theme Selector";
    int title_x = (screen_width - strlen(title)) / 2;
    ncplane_putstr_yx(stdplane_, 0, title_x > 0 ? title_x : 0, title);

    renderThemeList(list_plane, screen_height - 5);
    renderPreview(preview_plane, preview_theme);
    renderInstructions(instr_plane);

    notcurses_render(nc_);

    // Get input
    ncinput ni;
    memset(&ni, 0, sizeof(ni));
    uint32_t key = notcurses_get_blocking(nc_, &ni);

    if (key == (uint32_t)-1)
      continue;

    if (ni.id == NCKEY_UP || key == 'k') {
      selectPrevious();
    } else if (ni.id == NCKEY_DOWN || key == 'j') {
      selectNext();
    } else if (ni.id == NCKEY_ENTER || key == 10 || key == 13) {
      result = themes_[selected_index_];
      running = false;
    } else if (key == 'q' || key == 27) { // q or ESC
      running = false;
    }
  }

  // Cleanup
  ncplane_destroy(instr_plane);
  ncplane_destroy(preview_plane);
  ncplane_destroy(list_plane);

  return result;
}

} // namespace fx