#include "theme_loader.h"
#include <fstream>
#include <iostream>
#include <sstream>

flux::ThemeDefinition ThemeLoader::loadFromTOML(const std::string &path) {
  try {
    auto data = toml::parse_file(path);
    return parseThemeData(data);
  } catch (const std::exception &e) {
    std::cerr << "Error loading theme from " << path << ": " << e.what()
              << "\n";
    std::cerr << "Falling back to default theme\n";
    return flux::ThemeManager::getDefaultThemeDef();
  }
}

flux::ThemeDefinition
ThemeLoader::loadFromTOMLString(const std::string &toml_content) {
  try {
    auto data = toml::parse(toml_content);
    return parseThemeData(data);
  } catch (const std::exception &e) {
    std::cerr << "Error parsing TOML: " << e.what() << "\n";
    std::cerr << "Falling back to default theme\n";
    return flux::ThemeManager::getDefaultThemeDef();
  }
}

flux::ThemeDefinition ThemeLoader::parseThemeData(const toml::table &data) {
  flux::ThemeDefinition def;

  // Parse name
  if (auto name = data["name"].value<std::string>()) {
    def.name = *name;
  } else {
    def.name = "Custom Theme";
  }

  // Parse colors section
  if (auto colors = data["colors"].as_table()) {
    def.background = getColorOrDefault(*colors, "background", "transparent");
    def.foreground = getColorOrDefault(*colors, "foreground", "#FFFFFF");
    def.selected = getColorOrDefault(*colors, "selected", "#264F78");
    def.directory = getColorOrDefault(*colors, "directory", "#79C0FF");
    def.executable = getColorOrDefault(*colors, "executable", "#7EE787");
    def.hidden = getColorOrDefault(*colors, "hidden", "#6E7681");
    def.symlink = getColorOrDefault(*colors, "symlink", "#D2A8FF");
    def.parent_dir = getColorOrDefault(*colors, "parent_dir", "#D2A8FF");
    def.status_bar_bg = getColorOrDefault(*colors, "status_bar_bg", "#21262D");
    def.status_bar_fg = getColorOrDefault(*colors, "status_bar_fg", "#C9D1D9");
    def.status_bar_active =
        getColorOrDefault(*colors, "status_bar_active", "#58A6FF");
    def.ui_secondary = getColorOrDefault(*colors, "ui_secondary", "#8B949E");
    def.ui_border = getColorOrDefault(*colors, "ui_border", "#30363D");
    def.ui_error = getColorOrDefault(*colors, "ui_error", "#FF7B72");
    def.ui_warning = getColorOrDefault(*colors, "ui_warning", "#E3B341");
    def.ui_accent = getColorOrDefault(*colors, "ui_accent", "#D2A8FF");
    def.ui_info = getColorOrDefault(*colors, "ui_info", "#79C0FF");
    def.ui_success = getColorOrDefault(*colors, "ui_success", "#7EE787");
  }

  return def;
}

std::string ThemeLoader::getColorOrDefault(const toml::table &colors,
                                           const std::string &key,
                                           const std::string &default_val) {
  if (auto value = colors[key].value<std::string>()) {
    return *value;
  }
  return default_val;
}

std::vector<std::filesystem::path> ThemeLoader::getThemeSearchPaths() {
  std::vector<std::filesystem::path> paths;

  // 1. User config directory
#ifdef _WIN32
  if (const char *appdata = std::getenv("APPDATA")) {
    paths.push_back(std::filesystem::path(appdata) / "flux" / "themes");
  }
#else
  if (const char *home = std::getenv("HOME")) {
    paths.push_back(std::filesystem::path(home) / ".config" / "flux" /
                    "themes");
  }

  // 2. XDG config directory
  if (const char *xdg_config = std::getenv("XDG_CONFIG_HOME")) {
    paths.push_back(std::filesystem::path(xdg_config) / "flux" / "themes");
  }

  // 3. System-wide themes
  paths.push_back("/usr/share/flux/themes");
  paths.push_back("/usr/local/share/flux/themes");
#endif

  // 4. Current directory
  paths.push_back(std::filesystem::current_path() / "themes");

  return paths;
}

std::optional<std::string>
ThemeLoader::findThemeFile(const std::string &theme_name) {
  auto paths = getThemeSearchPaths();

  for (const auto &base_path : paths) {
    // Try with .toml extension
    auto toml_path = base_path / (theme_name + ".toml");
    if (std::filesystem::exists(toml_path)) {
      return toml_path.string();
    }

    // Try without extension
    auto no_ext_path = base_path / theme_name;
    if (std::filesystem::exists(no_ext_path)) {
      return no_ext_path.string();
    }
  }

  return std::nullopt;
}
