#pragma once

#include "flux/core/browser.h"
#include "flux/ui/icon_provider.h"
#include "flux/ui/theme.h"

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

namespace flux {

class Renderer {
public:
  Renderer();

  void render(const Browser &browser);
  void setTheme(const Theme &theme);
  void setIconStyle(IconStyle style);

  size_t getViewportHeight() const { return viewport_height_; }
  IconStyle getIconStyle() const { return icon_provider_.getStyle(); }

private:
  int height_, width_;
  size_t viewport_height_;
  Theme theme_;
  IconProvider icon_provider_;

  void renderHeader(const Browser &browser);
  void renderFileList(const Browser &browser);
  void renderStatus(const Browser &browser);
  std::string formatSize(uintmax_t size) const;
};

} // namespace flux