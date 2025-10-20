#include "include/config_loader.hpp"
#include "include/file_opener.hpp"
#include "include/theme_loader.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <flux.h>
#include <iostream>
#include <locale>

#ifdef _WIN32
#include <windows.h>
#define WIN32_LEAN_AND_MEAN
#include <curses.h>
#include <shellapi.h>
#else
#include <ncursesw/ncurses.h>
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

#ifndef _WIN32
struct termios original_termios;
static struct sigaction original_sigwinch;
static struct sigaction original_sigtstp;
static struct sigaction original_sigcont;
#endif

// Global variables for terminal state
static flux::ThemeManager *g_theme_manager = nullptr;
static std::string g_theme_name;
static flux::Theme g_theme;

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
  std::cout << "  j/k or ↑/↓       Navigate\n";
  std::cout << "  h or ←           Parent directory\n";
  std::cout << "  l or → or Enter  Open directory/file\n";
  std::cout << "  g/G              Top/Bottom\n";
  std::cout << "  .                Toggle hidden\n";
  std::cout << "  s                Cycle sort mode\n";
  std::cout << "  q                Quit\n";
}

void suspendTerminal() {
  // Save terminal mode
  def_prog_mode();

  // Clean up ncurses
  if (!isendwin()) {
    endwin();
  }

  // Reset terminal to normal mode
  std::cout << "\033[0m" << std::flush; // Reset attributes
}

void resumeTerminal() {
  // Small delay to let external program fully exit
  usleep(50000); // 50ms - much shorter than before!

  // Reset terminal completely
  std::cout << "\033c" << std::flush;

  // Reinitialize ncurses
  refresh(); // This calls doupdate() which reinitializes

  // Restore terminal mode
  reset_prog_mode();

  // Reconfigure ncurses
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  set_escdelay(25);

  // Reapply colors and theme
  if (has_colors() && g_theme_manager) {
    start_color();
    use_default_colors();

    auto theme_path = fx::ThemeLoader::findThemeFile(g_theme_name);
    if (theme_path) {
      auto def = fx::ThemeLoader::loadFromTOML(*theme_path);
      g_theme = g_theme_manager->applyThemeDefinition(def);

      if (def.background != "transparent" && def.background != "default" &&
          !def.background.empty()) {
        bkgd(COLOR_PAIR(g_theme.background));
      }
    }
  }

  // Force complete redraw
  clearok(stdscr, TRUE);
  refresh();
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

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  set_escdelay(25);

  if (has_colors()) {
    start_color();
    use_default_colors();
  }

  flux::Browser browser(start_path);
  flux::Renderer renderer;

  if (show_hidden)
    browser.toggleHidden();

  flux::ThemeManager theme_manager;
  flux::Theme theme;

  // Set up global variables for callbacks
  g_theme_manager = &theme_manager;
  g_theme_name = theme_name;

  // Set up FileOpener callbacks ONCE at startup
  fx::FileOpener::setTerminalSuspendCallback(suspendTerminal);
  fx::FileOpener::setTerminalResumeCallback(resumeTerminal);

  auto theme_path = fx::ThemeLoader::findThemeFile(theme_name);
  if (theme_path) {
    std::cerr << "[fx] Loading theme: " << theme_name << " from " << *theme_path
              << "\n";
    auto def = fx::ThemeLoader::loadFromTOML(*theme_path);
    theme = theme_manager.applyThemeDefinition(def);
    g_theme = theme;

    if (def.background != "transparent" && def.background != "default" &&
        !def.background.empty())
      bkgd(COLOR_PAIR(theme.background));
  } else {
    std::cerr << "[fx] Theme '" << theme_name << "' not found, using default\n";
    theme = theme_manager.applyThemeDefinition(
        flux::ThemeManager::getDefaultThemeDef());
    g_theme = theme;
    bkgd(COLOR_PAIR(theme.background));
  }

  renderer.setTheme(theme);
  renderer.setIconStyle(use_icons ? IconStyle::AUTO : IconStyle::ASCII);

  int ch;
  bool running = true;

  while (running) {
    browser.updateScroll(renderer.getViewportHeight());
    renderer.render(browser);
    ch = getch();

    switch (ch) {
    case KEY_UP:
    case 'k':
      browser.selectPrevious();
      break;
    case KEY_DOWN:
    case 'j':
      browser.selectNext();
      break;
    case KEY_HOME:
    case 'g':
      browser.selectFirst();
      break;
    case KEY_END:
    case 'G':
      browser.selectLast();
      break;
    case KEY_PPAGE:
    case CTRL('b'):
      browser.pageUp(renderer.getViewportHeight());
      break;
    case KEY_NPAGE:
    case CTRL('f'):
      browser.pageDown(renderer.getViewportHeight());
      break;
    case KEY_LEFT:
    case 'h':
      browser.navigateUp();
      break;
    case '.':
    case 'H':
      browser.toggleHidden();
      break;
    case 's':
      browser.cycleSortMode();
      break;
    case 'r':
    case KEY_F(5):
      browser.refresh();
      break;
    case 'q':
    case CTRL('q'):
    case 27:
      running = false;
      break;
    case KEY_RESIZE:
      clearok(stdscr, TRUE);
      break;
    case KEY_RIGHT:
    case KEY_ENTER:
    case 10:
    case 13: {
      if (browser.isSelectedDirectory()) {
        browser.navigateInto(browser.getSelectedIndex());
      } else {
        const std::string selected_file = browser.getSelectedPath()->string();
        openFileWithHandler(selected_file, config);
        // Refresh browser and renderer after returning
        browser.refresh();
        renderer.setTheme(g_theme);
      }
      break;
    }
    }
  }

  endwin();
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
    // Show error without messing up terminal state
    std::cerr << "\n[fx] Error: " << result.error_message << "\n";
    std::cerr << "Press Enter to continue...";
    std::cin.ignore();
    std::cin.get();
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

void setup_signal_handlers() {
#ifndef _WIN32
  sigaction(SIGWINCH, nullptr, &original_sigwinch);
  sigaction(SIGTSTP, nullptr, &original_sigtstp);
  sigaction(SIGCONT, nullptr, &original_sigcont);
#endif
}