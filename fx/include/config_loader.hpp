#pragma once

#include "../vendor/toml.hpp"
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace fx {

struct FileHandler {
  std::vector<std::string> extensions;
  std::string pattern;
  std::string mime_type;
  std::string command;
  bool terminal = false;
};

struct Config {
  // Paths
  fs::path config_root;
  fs::path themes_dir;

  // Layout
  int panels = 3;
  std::string mode = "miller";
  bool show_hidden = false;

  // Appearance
  std::string theme = "catppuccin";
  bool icons = true;
  std::string border_style = "rounded";

  // Behavior
  bool sort_dirs_first = true;
  bool case_sensitive = false;

  // File handlers
  std::string default_handler = "xdg-open";
  std::vector<FileHandler> handler_rules;

  // Keybindings
  std::vector<std::string> quit_keys;
  std::vector<std::string> open_keys;
  std::vector<std::string> up_keys;
};

class ConfigLoader {
public:
  // Find the config root directory (./config or ~/.config/fx)
  static std::optional<fs::path> find_config_root() {
    // 1. Check local development directory
    fs::path local_config = fs::current_path() / "config";
    if (fs::exists(local_config) && fs::is_directory(local_config)) {
      // std::cout << "[fx] Using local config directory: " << local_config
      //           << "\n";
      return local_config;
    }

    // 2. Check platform-specific config directories
    std::optional<fs::path> config_dir = get_platform_config_directory();
    if (config_dir) {
      fs::path user_config = *config_dir / "fx";
      if (fs::exists(user_config) && fs::is_directory(user_config)) {
        // std::cout << "[fx] Using user config directory: " << user_config
        //           << "\n";
        return user_config;
      }
    }

    return std::nullopt;
  }

  // Find the config.toml file
  static std::optional<fs::path> find_config_file() {
    auto root = find_config_root();
    if (!root)
      return std::nullopt;

    fs::path config_file = *root / "config.toml";
    if (fs::exists(config_file)) {
      return config_file;
    }

    return std::nullopt;
  }

  // Get themes directory (for use with ThemeLoader)
  static std::optional<fs::path> get_themes_directory() {
    auto root = find_config_root();
    if (!root)
      return std::nullopt;

    fs::path themes = *root / "themes";
    if (fs::exists(themes) && fs::is_directory(themes)) {
      return themes;
    }

    return std::nullopt;
  }

  // Load configuration
  static Config load() {
    auto config_path = find_config_file();
    if (!config_path) {
      std::cerr << "[fx] No config file found, using defaults\n";
      Config cfg;
      // Try to set themes_dir even with default config
      auto themes = get_themes_directory();
      if (themes) {
        cfg.themes_dir = *themes;
      }
      return cfg;
    }

    // std::cout << "[fx] Loading config from: " << *config_path << "\n";
    return load_from_file(*config_path);
  }

  static Config load_from_file(const fs::path &path) {
    try {
      auto data = toml::parse_file(path.string());
      Config cfg;

      // Set paths
      cfg.config_root = path.parent_path();
      cfg.themes_dir = cfg.config_root / "themes";

      // Layout section
      if (auto layout = data["layout"].as_table()) {
        if (auto panels = layout->get("panels")) {
          cfg.panels = panels->value_or(3);
        }
        if (auto mode = layout->get("mode")) {
          cfg.mode = mode->value_or("miller");
        }
        if (auto show_hidden = layout->get("show_hidden")) {
          cfg.show_hidden = show_hidden->value_or(false);
        }
      }

      // Appearance section
      if (auto appearance = data["appearance"].as_table()) {
        if (auto theme = appearance->get("theme")) {
          cfg.theme = theme->value_or("catppuccin");
        }
        if (auto icons = appearance->get("icons")) {
          cfg.icons = icons->value_or(true);
        }
        if (auto border_style = appearance->get("border_style")) {
          cfg.border_style = border_style->value_or("rounded");
        }
      }

      // Behavior section
      if (auto behavior = data["behavior"].as_table()) {
        if (auto sort_dirs_first = behavior->get("sort_dirs_first")) {
          cfg.sort_dirs_first = sort_dirs_first->value_or(true);
        }
        if (auto case_sensitive = behavior->get("case_sensitive")) {
          cfg.case_sensitive = case_sensitive->value_or(false);
        }
      }

      // File handlers section
      if (auto handlers = data["file_handlers"].as_table()) {
        if (auto default_handler = handlers->get("default")) {
          cfg.default_handler = default_handler->value_or("xdg-open");
        }

        // Parse file handler rules
        if (auto rules_node = handlers->get("rules")) {
          if (auto rules = rules_node->as_array()) {
            for (auto &&rule : *rules) {
              if (auto rule_table = rule.as_table()) {
                FileHandler handler;

                // Extensions
                if (auto exts_node = rule_table->get("extensions")) {
                  if (auto exts = exts_node->as_array()) {
                    for (auto &&ext : *exts) {
                      if (auto ext_str = ext.value<std::string>()) {
                        handler.extensions.push_back(*ext_str);
                      }
                    }
                  }
                }

                // Pattern
                if (auto pattern_node = rule_table->get("pattern")) {
                  if (auto pattern = pattern_node->value<std::string>()) {
                    handler.pattern = *pattern;
                  }
                }

                // MIME type
                if (auto mime_node = rule_table->get("mime_type")) {
                  if (auto mime = mime_node->value<std::string>()) {
                    handler.mime_type = *mime;
                  }
                }

                // Command and terminal
                if (auto cmd_node = rule_table->get("command")) {
                  if (auto cmd = cmd_node->value<std::string>()) {
                    handler.command = *cmd;
                  }
                }
                if (auto term_node = rule_table->get("terminal")) {
                  handler.terminal = term_node->value_or(false);
                }

                cfg.handler_rules.push_back(std::move(handler));
              }
            }
          }
        }
      }

      // Keybindings section
      if (auto keys = data["keybindings"].as_table()) {
        if (auto quit = keys->get("quit")) {
          cfg.quit_keys = parse_key_array(quit);
        }
        if (auto open = keys->get("open")) {
          cfg.open_keys = parse_key_array(open);
        }
        if (auto up = keys->get("up")) {
          cfg.up_keys = parse_key_array(up);
        }
      }

      return cfg;
    } catch (const std::exception &e) {
      std::cerr << "[fx] Error parsing config: " << e.what() << "\n";
      std::cerr << "[fx] Falling back to defaults\n";
      return Config{};
    }
  }

  // Initialize config structure in a directory
  static bool initialize_config(const fs::path &root_path,
                                bool copy_system_themes = true) {
    try {
      // Create directories
      fs::create_directories(root_path);
      fs::create_directories(root_path / "themes");

      // Copy system themes if they exist and user wants them
      if (copy_system_themes) {
        copy_system_themes_to_user(root_path / "themes");
      }

      // Create default config.toml
      std::ofstream config_file(root_path / "config.toml");
      if (!config_file)
        return false;

      config_file << R"([layout]
panels = 3
mode = "miller"
show_hidden = false

[appearance]
theme = "catppuccin"
icons = true
border_style = "rounded"

[behavior]
sort_dirs_first = true
case_sensitive = false

[file_handlers]
default = "xdg-open"

[[file_handlers.rules]]
extensions = ["cpp", "h", "c"]
command = "nvim"
terminal = true

[[file_handlers.rules]]
pattern = "*.md"
command = "glow"

[[file_handlers.rules]]
mime_type = "image/*"
command = "kitty +kitten icat"

[keybindings]
quit = ["q", "ESC"]
open = ["l", "RIGHT", "ENTER"]
up = ["k", "UP"]
)";

      std::cout << "[fx] Created config at: " << root_path / "config.toml"
                << "\n";
      std::cout << "[fx] Created themes directory at: " << root_path / "themes"
                << "\n";

      return true;
    } catch (const std::exception &e) {
      std::cerr << "[fx] Failed to initialize config: " << e.what() << "\n";
      return false;
    }
  }

  // Initialize global config
  static bool initialize_global_config() {
    auto config_dir = get_platform_config_directory();
    if (!config_dir) {
      std::cerr << "[fx] Could not determine config directory\n";
      return false;
    }

    return initialize_config(*config_dir / "fx");
  }

private:
  // Copy system themes to user directory
  static void copy_system_themes_to_user(const fs::path &user_themes_dir) {
    std::vector<fs::path> system_theme_paths = {"/usr/share/fx/themes",
                                                "/usr/local/share/fx/themes"};

// Also check relative to executable for portable installations
#ifdef _WIN32
    // Windows: check program files
    if (const char *program_files = std::getenv("ProgramFiles")) {
      system_theme_paths.push_back(fs::path(program_files) / "fx" / "themes");
    }
#endif

    for (const auto &system_path : system_theme_paths) {
      if (fs::exists(system_path) && fs::is_directory(system_path)) {
        std::cout << "[fx] Copying themes from: " << system_path << "\n";

        try {
          for (const auto &entry : fs::directory_iterator(system_path)) {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".toml") {
              fs::path dest = user_themes_dir / entry.path().filename();

              // Don't overwrite existing themes
              if (!fs::exists(dest)) {
                fs::copy_file(entry.path(), dest);
                std::cout << "[fx]   Copied: " << entry.path().filename()
                          << "\n";
              }
            }
          }
        } catch (const std::exception &e) {
          std::cerr << "[fx] Warning: Could not copy some themes: " << e.what()
                    << "\n";
        }

        break; // Use first valid system path
      }
    }
  }

  static std::optional<fs::path> get_platform_config_directory() {
#ifdef _WIN32
    // Windows: %APPDATA%
    const char *appdata = std::getenv("APPDATA");
    if (appdata) {
      return fs::path(appdata);
    }
#elif defined(__APPLE__)
    // macOS: ~/Library/Application Support
    const char *home = std::getenv("HOME");
    if (home) {
      return fs::path(home) / "Library" / "Application Support";
    }
#else
    // Linux/Unix: ~/.config or $XDG_CONFIG_HOME
    const char *xdg_config = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
      return fs::path(xdg_config);
    }

    const char *home = std::getenv("HOME");
    if (home) {
      return fs::path(home) / ".config";
    }
#endif
    return std::nullopt;
  }

  static std::vector<std::string> parse_key_array(toml::node *node) {
    std::vector<std::string> result;
    if (!node)
      return result;

    if (auto arr = node->as_array()) {
      for (auto &&item : *arr) {
        if (auto str = item.value<std::string>()) {
          result.push_back(*str);
        }
      }
    }
    return result;
  }
};

} // namespace fx