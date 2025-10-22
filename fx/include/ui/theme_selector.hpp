#pragma once

#include "include/theme_loader.hpp"
#include "theme.hpp"
#include <filesystem>
#include <notcurses/notcurses.h>
#include <string>
#include <vector>

namespace fx {

struct ThemeEntry {
  std::string name;
  std::filesystem::path path;
  ThemeDefinition definition;
};

class ThemeSelector {
public:
  ThemeSelector(notcurses *nc, ncplane *stdplane);

  // Show theme selector and return selected theme (or nullopt if cancelled)
  std::optional<ThemeEntry> show(const std::string &current_theme);

private:
  notcurses *nc_;
  ncplane *stdplane_;
  std::vector<ThemeEntry> themes_;
  size_t selected_index_;
  size_t scroll_offset_;

  void loadAvailableThemes();
  void render(const Theme &preview_theme, int viewport_height);
  void renderThemeList(ncplane *list_plane, int height);
  void renderPreview(ncplane *preview_plane, const Theme &theme);
  void renderInstructions(ncplane *plane);

  uint64_t makeChannels(uint32_t fg, uint32_t bg);
  void selectNext();
  void selectPrevious();
  int getViewportHeight() const;
};

} // namespace fx