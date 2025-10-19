#ifndef ICON_PROVIDER_H
#define ICON_PROVIDER_H

#include <string>
#include <unordered_map>

enum class IconStyle
{
  ASCII,      // Safe ASCII icons: +, *, @, etc.
  NERD_FONTS, // Unicode/Nerd Fonts icons
  AUTO        // Auto-detect based on terminal
};

class IconProvider
{
public:
  explicit IconProvider(IconStyle style = IconStyle::AUTO);

  // Get icons for different file types
  std::string getDirectoryIcon() const;
  std::string getParentIcon() const;
  std::string getExecutableIcon() const;
  std::string getSymlinkIcon() const;
  std::string getHiddenIcon() const;
  std::string getFileIcon(const std::string &filename) const;

  // Utility
  IconStyle getStyle() const { return current_style_; }
  bool isUsingNerdFonts() const
  {
    return current_style_ == IconStyle::NERD_FONTS;
  }

private:
  IconStyle current_style_;
  std::unordered_map<std::string, std::string> nerd_font_map_;

  // Helper methods
  bool detectUnicodeSupport() const;
  void initializeNerdFontMap();
  std::string getFileExtension(const std::string &filename) const;
};

#endif // ICON_PROVIDER_H