#pragma once

#include "flux/core/browser.h"
#include "icon_provider.hpp"
// #include "theme.h"
#include "theme.hpp"
#include <notcurses/notcurses.h>

namespace fx {

class Renderer {
public:
  Renderer(notcurses *nc, ncplane *stdplane);

  void render(const flux::Browser &browser);
  void setTheme(const fx::Theme &theme);
  void setIconStyle(IconStyle style);

  size_t getViewportHeight() const { return viewport_height_; }
  IconStyle getIconStyle() const { return icon_provider_.getStyle(); }

private:
  notcurses *nc_;
  ncplane *stdplane_;
  int viewport_height_;
  fx::Theme theme_;
  fx::IconProvider icon_provider_;

  void setColors(uint32_t fg_rgb);
  void setColors(uint32_t fg_rgb, uint32_t bg_rgb);
  void setColorsDefaultBg(uint32_t fg_rgb);
  void clearToEOL();
  int height_, width_;

  void renderHeader(const flux::Browser &browser);
  void renderFileList(const flux::Browser &browser);
  void renderStatus(const flux::Browser &browser);
  std::string formatSize(uintmax_t size) const;
};

} // namespace fx