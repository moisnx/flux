// fx/theme_loader.h
#pragma once

#include "vendor/toml.hpp"
#include <filesystem>
#include <flux.h>
#include <string>

namespace fx {

class ThemeLoader {
public:
  static flux::ThemeDefinition loadFromTOML(const std::string &path);
  static flux::ThemeDefinition
  loadFromTOMLString(const std::string &toml_content);
  static std::vector<std::filesystem::path> getThemeSearchPaths();
  static std::optional<std::string>
  findThemeFile(const std::string &theme_name);

private:
  static flux::ThemeDefinition parseThemeData(const toml::table &data);
  static std::string getColorOrDefault(const toml::table &colors,
                                       const std::string &key,
                                       const std::string &default_val);
};
} // namespace fx