#include "flux/core/file_clipboard.h"
#include "include/config_loader.hpp"
#include "include/file_opener.hpp"
#include "include/input_prompt.hpp"
#include "include/theme_loader.hpp"
#include "include/ui/notification.hpp"
#include "include/ui/renderer.hpp"
#include "include/ui/theme.hpp"
#include "include/ui/theme_selector.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <flux.h>
#include <iostream>
#include <vector>

// REPLACE ncurses includes with notcurses
#ifdef _WIN32
#error "Notcurses migration not yet supported on Windows"
#else
#include <notcurses/notcurses.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifndef CTRL
#define CTRL(c) ((c) & 0x1f)
#endif

void setup_terminal_attributes();
void restore_terminal_attributes();
void setup_signal_handlers();
void openFileWithHandler(const std::string &filePath, const fx::Config &config);
std::optional<fx::FileHandler> findMatchingHandler(const std::string &filePath,
                                                   const fx::Config &config);

// Global variables for terminal state
#ifndef _WIN32
struct termios original_termios;
static struct sigaction original_sigwinch;
static struct sigaction original_sigtstp;
static struct sigaction original_sigcont;
#endif

// UPDATED globals for notcurses
static notcurses *g_nc = nullptr;
static ncplane *g_stdplane = nullptr;
static fx::ThemeManager *g_theme_manager = nullptr;
static fx::NotificationManager *g_notif_manager = nullptr;
static std::string g_theme_name;
static fx::Theme g_theme;

void printUsage(const char *program_name) {
  std::cout << "fx - A modern terminal file browser\n\n";
  std::cout << "Usage: " << program_name << " [OPTIONS] [PATH]\n\n";
  std::cout << "Options:\n";
  std::cout << "  -h, --help          Show this help message\n";
  std::cout << "  -v, --version       Show version information\n";
  std::cout << "  --init-config       Initialize config directory\n";
  std::cout << "  --theme NAME        Override theme from config\n";
  std::cout << "  --no-icons          Disable icons (use ASCII only)\n";
  std::cout << "  --show-hidden       Show hidden files on startup\n";
  std::cout << "  --strict            Enable command whitelist validation\n\n";
  std::cout << "Configuration:\n";
  std::cout << "  Local:  ./config/config.toml\n";
  std::cout << "  Global: ~/.config/fx/config.toml\n\n";
  std::cout << "Security:\n";
  std::cout << "  Commands are executed without shell interpretation\n";
  std::cout << "  Use --strict to enforce command whitelist\n\n";
  std::cout << "Keybindings:\n";
  std::cout << "  Navigation:\n";
  std::cout << "    j/k or ↑/↓         Navigate up/down\n";
  std::cout << "    h or ←             Parent directory\n";
  std::cout << "    l or → or Enter    Open directory/file\n";
  std::cout << "    g/G or Home/End    Jump to top/bottom\n";
  std::cout << "    Ctrl+u / Ctrl+d    Half page up/down\n";
  std::cout << "    Ctrl+b / Ctrl+f    Full page up/down\n";
  std::cout << "    PgUp / PgDn        Page up/down\n\n";
  std::cout << "  File Operations:\n";
  std::cout << "    n                  Create new file\n";
  std::cout << "    N                  Create new directory\n";
  std::cout << "    r                  Rename selected item\n";
  std::cout << "    d                  Delete selected item\n\n";
  std::cout << "  View Options:\n";
  std::cout << "    .                  Toggle hidden files\n";
  std::cout << "    s                  Cycle sort mode\n";
  std::cout << "    R or F5            Refresh directory\n";
  std::cout << "    T                  Open theme selector\n\n";
  std::cout << "  Exit:\n";
  std::cout << "    q / Ctrl+q / Esc   Quit application\n";
}

void suspendTerminal() {
#ifndef _WIN32
  struct sigaction ignore_action;
  ignore_action.sa_handler = SIG_IGN;
  sigemptyset(&ignore_action.sa_mask);
  ignore_action.sa_flags = 0;

  sigaction(SIGWINCH, &ignore_action, nullptr);
  sigaction(SIGTSTP, &ignore_action, nullptr);
  sigaction(SIGCONT, &ignore_action, nullptr);
  sigaction(SIGTTIN, &ignore_action, nullptr);
  sigaction(SIGTTOU, &ignore_action, nullptr);
#endif

  // IMPORTANT: Clean up notification manager's plane first
  if (g_notif_manager) {
    g_notif_manager->cleanup(); // Add this method
  }

  // Stop notcurses completely to give terminal to external program
  if (g_nc) {
    notcurses_stop(g_nc);
    g_nc = nullptr;
    g_stdplane = nullptr;
  }
}

void resumeTerminal() {
  // Wait for terminal to settle after external program exits
  usleep(100000); // Increased from 50000 to 100000

  bool has_truecolor = false;
  if (const char *colorterm = std::getenv("COLORTERM")) {
    std::string ct = colorterm;
    has_truecolor = (ct == "truecolor" || ct == "24bit");
  }

  const char *termtype = nullptr;

  if (has_truecolor) {
    if (const char *term = std::getenv("TERM")) {
      std::string t = term;
      if (t.find("kitty") != std::string::npos ||
          t.find("konsole") != std::string::npos || t == "xterm-direct") {
        termtype = "xterm-direct";
      }
    }
  }

  // Reinitialize notcurses
  struct notcurses_options opts = {.termtype = termtype,
                                   .loglevel = NCLOGLEVEL_SILENT,
                                   .margin_t = 0,
                                   .margin_r = 0,
                                   .margin_b = 0,
                                   .margin_l = 0,
                                   .flags = NCOPTION_SUPPRESS_BANNERS |
                                            NCOPTION_NO_CLEAR_BITMAPS};

  g_nc = notcurses_init(&opts, nullptr);
  if (!g_nc) {
    std::cerr << "Failed to reinitialize notcurses\n";
    return;
  }

  g_stdplane = notcurses_stdplane(g_nc);
  if (!g_stdplane) {
    std::cerr << "Failed to get stdplane\n";
    return;
  }

  // Update notification manager BEFORE doing anything else
  if (g_notif_manager) {
    g_notif_manager->updateNotcursesPointer(g_nc);
  }

  // Reapply theme
  if (g_theme_manager && !g_theme_name.empty()) {
    auto theme_path = fx::ThemeLoader::findThemeFile(g_theme_name);
    if (theme_path) {
      auto def = fx::ThemeLoader::loadFromTOML(*theme_path);
      g_theme = g_theme_manager->applyThemeDefinition(def);

      if (def.background != "transparent" && def.background != "default" &&
          !def.background.empty()) {
        uint64_t channels = 0;
        ncchannels_set_fg_rgb8(&channels, (g_theme.foreground >> 16) & 0xFF,
                               (g_theme.foreground >> 8) & 0xFF,
                               g_theme.foreground & 0xFF);
        ncchannels_set_bg_rgb8(&channels, (g_theme.background >> 16) & 0xFF,
                               (g_theme.background >> 8) & 0xFF,
                               g_theme.background & 0xFF);
        ncplane_set_base(g_stdplane, " ", 0, channels);
      }
    }
  }

  // Single refresh
  notcurses_refresh(g_nc, nullptr, nullptr);

#ifndef _WIN32
  // Unblock signals
  sigset_t mask;
  sigemptyset(&mask);
  sigprocmask(SIG_SETMASK, &mask, nullptr);
#endif
}

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--init-config") {
      std::cout << "Initializing global config...\n";
      if (fx::ConfigLoader::initialize_global_config()) {
        std::cout << "✓ Config initialized at ~/.config/fx/\n";
        return 0;
      } else {
        std::cerr << "✗ Failed to initialize config\n";
        return 1;
      }
    }

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    }

    if (arg == "-v" || arg == "--version") {
      std::cout << "fx (Flux) version " << flux::getVersion() << "\n";
      return 0;
    }
  }

  fx::Config config = fx::ConfigLoader::load();

  std::string start_path = ".";
  std::string theme_name = config.theme;
  bool use_icons = config.icons;
  bool show_hidden = config.show_hidden;
  bool strict_mode = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--theme" && i + 1 < argc)
      theme_name = argv[++i];
    else if (arg == "--no-icons")
      use_icons = false;
    else if (arg == "--show-hidden")
      show_hidden = true;
    else if (arg == "--strict")
      strict_mode = true;
    else if (arg[0] != '-')
      start_path = arg;
    else if (arg != "-h" && arg != "--help" && arg != "-v" &&
             arg != "--version" && arg != "--init-config") {
      std::cerr << "Unknown option: " << arg << "\n";
      std::cerr << "Try '" << argv[0] << " --help'\n";
      return 1;
    }
  }

  if (strict_mode) {
    std::cerr << "[fx] Strict mode: command whitelist enabled\n";
    fx::FileOpener::enableWhitelist(true);
  }

  std::setlocale(LC_ALL, "");

  setup_terminal_attributes();
  setup_signal_handlers();

  bool has_truecolor = false;
  if (const char *colorterm = std::getenv("COLORTERM")) {
    std::string ct = colorterm;
    has_truecolor = (ct == "truecolor" || ct == "24bit");
  }

  // Only set termtype if we're confident it's supported
  // Let notcurses auto-detect for most terminals
  const char *termtype = nullptr;

  // Only override for terminals we know support xterm-direct
  if (has_truecolor) {
    if (const char *term = std::getenv("TERM")) {
      std::string t = term;
      // Only use xterm-direct for known-good terminals
      if (t.find("kitty") != std::string::npos ||
          t.find("konsole") != std::string::npos || t == "xterm-direct") {
        termtype = "xterm-direct";
      }
      // For other terminals with truecolor, let notcurses auto-detect
    }
  }

  struct notcurses_options opts = {.termtype = termtype,
                                   .loglevel = NCLOGLEVEL_SILENT,
                                   .margin_t = 0,
                                   .margin_r = 0,
                                   .margin_b = 0,
                                   .margin_l = 0,
                                   .flags = NCOPTION_SUPPRESS_BANNERS |
                                            NCOPTION_NO_CLEAR_BITMAPS};

  notcurses *nc = notcurses_init(&opts, nullptr);
  if (!nc) {
    std::cerr << "Failed to initialize notcurses\n";
    return 1;
  }

  if (!notcurses_cantruecolor(nc)) {
    std::cerr << "Warning: Terminal doesn't support true color!\n";
    std::cerr << "Colors may appear incorrect.\n";
  }
  ncplane *stdplane = notcurses_stdplane(nc);

  g_nc = nc;
  g_stdplane = stdplane;

  flux::Browser browser(start_path);
  fx::Renderer renderer(nc, stdplane);

  if (show_hidden)
    browser.toggleHidden();

  fx::ThemeManager theme_manager;
  fx::Theme theme;

  // Set up global variables for callbacks
  g_theme_manager = &theme_manager;
  g_theme_name = theme_name;

  fx::FileOpener::setTerminalSuspendCallback(suspendTerminal);
  fx::FileOpener::setTerminalResumeCallback(resumeTerminal);

  // Theme loading stays mostly the same
  auto theme_path = fx::ThemeLoader::findThemeFile(theme_name);
  if (theme_path) {
    auto def = fx::ThemeLoader::loadFromTOML(*theme_path);
    theme = theme_manager.applyThemeDefinition(def);
    g_theme = theme;

    // Set the background for the entire stdplane
    if (def.background != "transparent" && def.background != "default" &&
        !def.background.empty()) {
      uint64_t channels = 0;
      ncchannels_set_fg_rgb8(&channels, (theme.foreground >> 16) & 0xFF,
                             (theme.foreground >> 8) & 0xFF,
                             theme.foreground & 0xFF);
      ncchannels_set_bg_rgb8(&channels, (theme.background >> 16) & 0xFF,
                             (theme.background >> 8) & 0xFF,
                             theme.background & 0xFF);
      ncplane_set_base(stdplane, " ", 0, channels);
    }
  }

  fx::NotificationManager notif_manager(nc, theme);

  g_notif_manager = &notif_manager;

  fx::StatusBar status_bar(theme);
  fx::InputPrompt::setTheme(theme);
  renderer.setTheme(theme);
  renderer.setIconStyle(use_icons ? fx::IconStyle::AUTO : fx::IconStyle::ASCII);

  ncinput ni;
  bool running = true;
  bool needs_render = true; // Browser content needs re-render
  // bool  // Notifications need re-render
  static char last_key = 0;
  static auto last_key_time = std::chrono::steady_clock::now();

  while (running) {
    if (needs_render) {
      browser.updateScroll(renderer.getViewportHeight());
      ncplane_erase(stdplane);
      renderer.render(browser);
      notif_manager.render(stdplane, fx::NotificationPosition::BOTTOM);
      notcurses_render(nc);
      needs_render = false;
    }

    ncinput ni;
    memset(&ni, 0, sizeof(ni));

    // Calculate timeout to next notification expiry
    struct timespec timeout;
    auto next_expiry = notif_manager.getNextExpiryTime();

    if (next_expiry.has_value()) {
      auto now = std::chrono::steady_clock::now();
      auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                           *next_expiry - now)
                           .count();

      if (wait_time <= 0) {
        // Notification expired - prune and re-render ONCE
        notif_manager.pruneExpired();
        needs_render = true;
        continue;
      }

      timeout.tv_sec = wait_time / 1000;
      timeout.tv_nsec = (wait_time % 1000) * 1000000;
    } else {
      // No notifications, block indefinitely
      timeout.tv_sec = 3600;
      timeout.tv_nsec = 0;
    }

    uint32_t key = notcurses_get(nc, &timeout, &ni);

    // Timeout - check for expired notifications
    if (key == (uint32_t)-1 || key == 0) {
      size_t before = notif_manager.getNotificationCount();
      notif_manager.pruneExpired();
      size_t after = notif_manager.getNotificationCount();

      if (before != after) {
        needs_render = true;
      }
      continue;
    }

    // Browser navigation - only browser needs re-render
    needs_render = true; // Default for most keys

    // Handle special keys
    if (ni.id == NCKEY_RESIZE) {
      ncplane_erase(stdplane);
      browser.updateScroll(renderer.getViewportHeight());
      notif_manager.handleResize();
      needs_render = true;
    } else if (ni.id == NCKEY_UP) {
      browser.selectPrevious();
    } else if (ni.id == NCKEY_DOWN) {
      browser.selectNext();
    } else if (ni.id == NCKEY_LEFT) {
      browser.navigateUp();
    } else if (ni.id == NCKEY_RIGHT) {
      if (browser.isSelectedDirectory()) {
        browser.navigateInto(browser.getSelectedIndex());
      } else {
        const std::string selected_file = browser.getSelectedPath()->string();
        size_t saved_index = browser.getSelectedIndex();
        openFileWithHandler(selected_file, config);
        browser.updateScroll(renderer.getViewportHeight());
        browser.refresh();
        browser.selectByIndex(saved_index);
        renderer.setTheme(g_theme);
        notif_manager.setTheme(g_theme);
      }
    } else if (ni.id == NCKEY_ENTER) {
      if (browser.isSelectedDirectory()) {
        browser.navigateInto(browser.getSelectedIndex());
      } else {
        const std::string selected_file = browser.getSelectedPath()->string();
        size_t saved_index = browser.getSelectedIndex();
        openFileWithHandler(selected_file, config);
        browser.updateScroll(renderer.getViewportHeight());
        browser.refresh();
        browser.selectByIndex(saved_index);
        renderer.setTheme(g_theme);
        notif_manager.setTheme(g_theme);
      }
    } else if (ni.id == NCKEY_HOME) {
      browser.selectFirst();
    } else if (ni.id == NCKEY_END) {
      browser.selectLast();
    } else if (ni.id == NCKEY_PGUP) {
      browser.pageUp(renderer.getViewportHeight());
    } else if (ni.id == NCKEY_PGDOWN) {
      browser.pageDown(renderer.getViewportHeight());
    } else if (ni.id == NCKEY_F05) {
      browser.refresh();
      notif_manager.success("Directory refreshed", 1500);
      // New notification added
    }
    // Handle regular character keys
    else if (key == 'k') {
      browser.selectPrevious();
    } else if (key == 'j') {
      browser.selectNext();
    } else if (key == 'h') {
      browser.navigateUp();
    } else if (key == 'l' || key == 10 || key == 13) {
      if (browser.isSelectedDirectory()) {
        browser.navigateInto(browser.getSelectedIndex());
      } else {
        const std::string selected_file = browser.getSelectedPath()->string();
        size_t saved_index = browser.getSelectedIndex();
        openFileWithHandler(selected_file, config);
        browser.updateScroll(renderer.getViewportHeight());
        browser.refresh();
        browser.selectByIndex(saved_index);
        renderer.setTheme(g_theme);
        notif_manager.setTheme(g_theme);
      }
    } else if (key == 'g') {
      browser.selectFirst();
    } else if (key == 'G') {
      browser.selectLast();
    } else if (key == CTRL('u')) {
      int half_page = renderer.getViewportHeight() / 2;
      for (int i = 0; i < half_page; ++i) {
        browser.selectPrevious();
      }
    } else if (key == CTRL('d')) {
      int half_page = renderer.getViewportHeight() / 2;
      for (int i = 0; i < half_page; ++i) {
        browser.selectNext();
      }
    } else if (key == CTRL('b')) {
      browser.pageUp(renderer.getViewportHeight());
    } else if (key == CTRL('f')) {
      browser.pageDown(renderer.getViewportHeight());
    } else if (key == '.') {
      browser.toggleHidden();
      notif_manager.info("Hidden files toggled", 1500);
      // New notification
    } else if (key == 's') {
      browser.cycleSortMode();
      notif_manager.info("Sort mode changed", 1500);
      // New notification
    } else if (key == 'R') {
      browser.refresh();
      notif_manager.success("Directory refreshed", 1500);
      // New notification
    } else if (key == 'q' || key == CTRL('q')) {
      running = false;
      needs_render = false; // Don't render on exit
    }
    // Handle ESC
    else if (key == 27) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - last_key_time)
                         .count();

      if (last_key != 27 || elapsed > 300) {
        running = false;
        needs_render = false;
      }
      last_key = 27;
      last_key_time = now;
    } else if (key == 'n') {
      auto name = fx::InputPrompt::getString(nc, stdplane, "New file: ");

      if (name) {
        if (browser.createFile(*name)) {
          notif_manager.success("Created file: " + *name);
          // New notification
        } else {
          notif_manager.error(
              "Failed to create file: " + browser.getErrorMessage(), 5000);
        }
      }

    } else if (key == 'N') {
      auto name = fx::InputPrompt::getString(nc, stdplane, "New directory: ");
      if (name) {
        if (browser.createDirectory(*name)) {
          notif_manager.success("Created directory: " + *name);

        } else {
          notif_manager.error(
              "Failed to create directory: " + browser.getErrorMessage(), 5000);
        }
      }
    } else if (key == 'r') {
      const std::string default_name =
          browser.getEntryByIndex(browser.getSelectedIndex())->name;
      auto name =
          fx::InputPrompt::getString(nc, stdplane, "Rename: ", default_name);
      if (name) {
        if (browser.renameEntry(browser.getSelectedIndex(), *name)) {
          notif_manager.success("Renamed to: " + *name);

        } else {
          notif_manager.error("Failed to rename: " + browser.getErrorMessage(),
                              5000);
        }
      }
    } else if (key == 'd') {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - last_key_time)
                         .count();

      if (last_key == 'd' && elapsed < 500) {
        const std::string filename =
            browser.getEntryByIndex(browser.getSelectedIndex())->name;

        ncplane_erase(stdplane);
        renderer.render(browser);
        notcurses_render(nc);

        bool confirmed = fx::MessageBox::confirm(
            nc, stdplane, "Delete File",
            "Are you sure you want to delete '" + filename + "'?", theme);

        if (confirmed) {
          if (browser.removeEntry(browser.getSelectedIndex())) {
            notif_manager.success("Deleted: " + filename);

          } else {
            notif_manager.error(
                "Failed to delete: " + browser.getErrorMessage(), 5000);
          }
        } else {
          notif_manager.info("Delete cancelled", 2000);
        }

        last_key = 0;
      } else {
        last_key = 'd';
        last_key_time = now;
        notif_manager.hint("Press 'd' again to delete", 1500);
        // New notification
      }
    } else if (key == 'T') {
      ncplane_erase(stdplane);
      renderer.render(browser);
      notcurses_render(nc);

      fx::ThemeSelector selector(nc, stdplane);
      auto selected_theme = selector.show(theme_name);

      if (selected_theme) {
        g_theme_name = selected_theme->name;
        theme_name = selected_theme->name;
        theme = theme_manager.applyThemeDefinition(selected_theme->definition);
        g_theme = theme;

        fx::InputPrompt::setTheme(theme);
        renderer.setTheme(theme);
        notif_manager.setTheme(theme);
        status_bar.setTheme(theme);

        if (selected_theme->definition.background != "transparent" &&
            selected_theme->definition.background != "default" &&
            !selected_theme->definition.background.empty()) {
          uint64_t channels = 0;
          ncchannels_set_fg_rgb8(&channels, (theme.foreground >> 16) & 0xFF,
                                 (theme.foreground >> 8) & 0xFF,
                                 theme.foreground & 0xFF);
          ncchannels_set_bg_rgb8(&channels, (theme.background >> 16) & 0xFF,
                                 (theme.background >> 8) & 0xFF,
                                 theme.background & 0xFF);
          ncplane_set_base(stdplane, " ", 0, channels);

          if (!fx::ConfigLoader::save_theme(selected_theme->name)) {
            notif_manager.warning("Theme applied but not saved", 3000);

          } else {
            notif_manager.success("Theme: " + selected_theme->name);
          }
        }
        browser.updateScroll(renderer.getViewportHeight());
      }

      last_key = 0;
    } else if (key == 'Y') {
      bool copied =
          flux::FileClipboard::copyFiles({browser.getSelectedPath()->string()});

      if (copied) {
        const std::string filename =
            browser.getEntryByIndex(browser.getSelectedIndex())->name;
        notif_manager.success("Copied: " + filename);

      } else {
        notif_manager.error("Failed to copy file", 3000);
      }
    } else if (key == 'p') {
      std::vector<std::string> paths = flux::FileClipboard::getFiles();
      if (paths.empty()) {
        notif_manager.warning("Clipboard is empty");

      } else if (browser.executePaste(paths, false)) {
        notif_manager.success("Pasted " + std::to_string(paths.size()) +
                              " item(s)");

      } else {
        notif_manager.error("Failed to paste: " + browser.getErrorMessage(),
                            5000);
      }
    } else {
      // Unknown key - don't re-render anything
      needs_render = false;
    }
  }

  // RESERVED for future features:
  // 'a' - reserved for mark/select files
  // 't' - reserved for tabs or tree view
  // 'H' - reserved for home directory
  // '/' - reserved for search/filter
  // '?' - reserved for help overlay

  notcurses_stop(nc);
  g_nc = nullptr;
  g_stdplane = nullptr;

  restore_terminal_attributes();
  return 0;
}

std::optional<fx::FileHandler> findMatchingHandler(const std::string &filePath,
                                                   const fx::Config &config) {
  std::filesystem::path path(filePath);
  std::string extension = path.extension().string();

  if (!extension.empty() && extension[0] == '.')
    extension = extension.substr(1);

  for (const auto &handler : config.handler_rules) {
    for (const auto &ext : handler.extensions)
      if (extension == ext)
        return handler;

    if (!handler.pattern.empty() && handler.pattern.substr(0, 2) == "*.") {
      std::string pattern_ext = handler.pattern.substr(2);
      if (extension == pattern_ext)
        return handler;
    }
  }

  return std::nullopt;
}

void openFileWithHandler(const std::string &filePath,
                         const fx::Config &config) {
  auto handler = findMatchingHandler(filePath, config);

  fx::FileOpener::OpenResult result;

  if (handler) {
    fx::FileOpener::OpenConfig open_config;
    open_config.command = handler->command;
    open_config.wait_for_completion = handler->terminal;
    open_config.validate_command = fx::FileOpener::isWhitelistEnabled();

    std::cerr << "[fx] Opening with handler: " << handler->command << "\n";
    result = fx::FileOpener::openWith(filePath, open_config);
  } else {
    std::cerr << "[fx] Opening with default handler\n";
    result = fx::FileOpener::openWithDefault(filePath);
  }

  if (!result.success) {
    // Ensure terminal is in a clean state before showing error
    if (g_nc != nullptr) {
      suspendTerminal();
    }

    std::cerr << "\n╭─────────────────────────────────────╮\n";
    std::cerr << "│ Error Opening File                  │\n";
    std::cerr << "╰─────────────────────────────────────╯\n";
    std::cerr << "\n" << result.error_message << "\n\n";
    std::cerr << "Press Enter to continue...";
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();

    // Resume terminal
    resumeTerminal();
  }
}

void setup_terminal_attributes() {
#ifndef _WIN32
  tcgetattr(STDIN_FILENO, &original_termios);
  struct termios new_termios = original_termios;
  new_termios.c_iflag &= ~IXON;
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
#endif
}

void restore_terminal_attributes() {
#ifndef _WIN32
  tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
#endif
}

#ifndef _WIN32
static volatile sig_atomic_t terminal_suspended = 0;

void handle_sigtstp(int sig) {
  if (!terminal_suspended) {
    terminal_suspended = 1;
    suspendTerminal();
    signal(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
  }
}

void handle_sigcont(int sig) {
  signal(SIGTSTP, handle_sigtstp);
  if (terminal_suspended) {
    terminal_suspended = 0;
    resumeTerminal();
  }
}

void handle_sigwinch(int sig) {
  if (!terminal_suspended && g_nc && g_stdplane) {
    // Notcurses handles the actual resize, we just need to clear artifacts
    ncplane_erase(g_stdplane);
    notcurses_refresh(g_nc, nullptr, nullptr);
  }
}
#endif

void setup_signal_handlers() {
#ifndef _WIN32
  struct sigaction sa_tstp, sa_cont, sa_winch;

  // Save originals
  sigaction(SIGWINCH, nullptr, &original_sigwinch);
  sigaction(SIGTSTP, nullptr, &original_sigtstp);
  sigaction(SIGCONT, nullptr, &original_sigcont);

  // Setup SIGTSTP handler
  sa_tstp.sa_handler = handle_sigtstp;
  sigemptyset(&sa_tstp.sa_mask);
  sa_tstp.sa_flags = SA_RESTART;
  sigaction(SIGTSTP, &sa_tstp, nullptr);

  // Setup SIGCONT handler
  sa_cont.sa_handler = handle_sigcont;
  sigemptyset(&sa_cont.sa_mask);
  sa_cont.sa_flags = SA_RESTART;
  sigaction(SIGCONT, &sa_cont, nullptr);

  // Setup SIGWINCH handler
  sa_winch.sa_handler = handle_sigwinch;
  sigemptyset(&sa_winch.sa_mask);
  sa_winch.sa_flags = SA_RESTART;
  sigaction(SIGWINCH, &sa_winch, nullptr);
#endif
}