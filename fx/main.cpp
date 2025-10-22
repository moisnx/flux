#include "include/config_loader.hpp"
#include "include/file_opener.hpp"
#include "include/input_prompt.hpp"
#include "include/theme_loader.hpp"

// #include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <flux.h>
#include <iostream>
// #include <locale>

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
#ifndef _WIN32
  // Use SIG_IGN instead of blocking - better for signal propagation
  struct sigaction ignore_action;
  ignore_action.sa_handler = SIG_IGN;
  sigemptyset(&ignore_action.sa_mask);
  ignore_action.sa_flags = 0;

  // These will be restored when we resume
  sigaction(SIGWINCH, &ignore_action, nullptr);
  sigaction(SIGTSTP, &ignore_action, nullptr);
  sigaction(SIGCONT, &ignore_action, nullptr);
  sigaction(SIGTTIN, &ignore_action, nullptr);
  sigaction(SIGTTOU, &ignore_action, nullptr);
#endif

  // Save terminal mode
  def_prog_mode();

  // Clean up ncurses gracefully with neutral colors
  if (!isendwin()) {
    attrset(A_NORMAL);
    bkgd(COLOR_PAIR(0));
    clear();
    refresh();
    endwin();
  }

  // Ensure clean terminal state
  std::cout << "\033[0m"     // Reset all attributes
            << "\033[39;49m" // Default fg/bg colors
            << "\033[2J"     // Clear screen
            << "\033[H"      // Home cursor
            << "\033[?25h"   // Show cursor
            << std::flush;

  // usleep(\1); // 5ms
}

void resumeTerminal() {
  // Gently clear the screen
  std::cout << "\033[2J\033[H" << std::flush;

  // Reinitialize ncurses
  refresh();
  reset_prog_mode();

  // Reconfigure ncurses settings
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  set_escdelay(25);

  // Clear any pending input
  flushinp();

  // Reapply colors and theme (OPTIMIZED - no file I/O)
  if (has_colors() && g_theme_manager) {
    start_color();
    use_default_colors();

    // Just reapply the cached theme directly - no disk access!
    if (g_theme.background > 0) {
      bkgd(COLOR_PAIR(g_theme.background));
    }
  }

  // Force a clean redraw
  clearok(stdscr, TRUE);
  clear();
  refresh();

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

  fx::InputPrompt::setTheme(theme);
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
    case 'R':
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

        // Save the current scroll position
        size_t saved_index = browser.getSelectedIndex();

        openFileWithHandler(selected_file, config);

        // After returning, force the browser to recalculate scroll
        browser.updateScroll(renderer.getViewportHeight());
        // Force a complete redraw
        clearok(stdscr, TRUE);
        clear();

        // Refresh browser and renderer
        browser.refresh();
        browser.selectByIndex(saved_index);
        renderer.setTheme(g_theme);
      }
      break;
    }

    case 'a': // Create file
    case 'n': {
      auto name = fx::InputPrompt::getString("New file: ");
      if (name) {
        if (browser.createFile(*name)) {
          // Success - file created and selected
        } else {
          const std::string selected_file = browser.getSelectedPath()->string();
          size_t saved_index = browser.getSelectedIndex();

          openFileWithHandler(selected_file, config);

          // Restore position first, then update scroll
          browser.selectByIndex(saved_index);
          browser.updateScroll(renderer.getViewportHeight());

          // Clean redraw
          clearok(stdscr, TRUE);
          clear();
          renderer.setTheme(g_theme);
        }
      }
      break;
    }
    case 'A': // Create directory
    case 'N': {
      auto name = fx::InputPrompt::getString("New directory: ");
      if (name) {
        if (browser.createDirectory(*name)) {
          // Success - directory created and selected
        } else {
          // Show error
          std::string error = browser.getErrorMessage();
          move(LINES - 1, 0);
          clrtoeol();
          attron(A_BOLD | COLOR_PAIR(1));
          mvprintw(LINES - 1, 0, "Error: %s", error.c_str());
          attroff(A_BOLD | COLOR_PAIR(1));
          refresh();
          napms(2000);
        }
      }
      break;
    }
    case 'r': {
      const std::string default_name =
          browser.getEntryByIndex(browser.getSelectedIndex())->name;
      auto name = fx::InputPrompt::getString("Rename: ", default_name);
      if (name) {
        if (browser.renameEntry(browser.getSelectedIndex(), *name)) {
          // Success
        } else {
          std::string error = browser.getErrorMessage();
          move(LINES - 1, 0);
          clrtoeol();
          attron(A_BOLD | COLOR_PAIR(1)); // Assuming color pair 1 is red/error
          mvprintw(LINES - 1, 0, "Error: %s", error.c_str());
          attroff(A_BOLD | COLOR_PAIR(1));
          refresh();
          napms(2000); // Show error for 2 seconds
        }
      }
      break;
    }
    case 'd': {

      if (fx::InputPrompt::getConfirmation(
              "Do you want to remove this file/folder? ")) {
        if (browser.removeEntry(browser.getSelectedIndex())) {
          // Success
        } else {
          std::string error = browser.getErrorMessage();
          move(LINES - 1, 0);
          clrtoeol();
          attron(A_BOLD | COLOR_PAIR(1)); // Assuming color pair 1 is
          mvprintw(LINES - 1, 0, "Error: %s", error.c_str());
          attroff(A_BOLD | COLOR_PAIR(1));
          refresh();
          napms(2000); // Show error for 2 seconds
        }
      }
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
    // Ensure terminal is in a clean state before showing error
    if (!isendwin()) {
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
// Add these global variables
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
  // Only handle resize if we're active
  if (!terminal_suspended) {
    endwin();
    refresh();
    clearok(stdscr, TRUE);
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