#include "include/config_loader.hpp"
#include "include/file_opener.hpp"
#include "include/input_prompt.hpp"
#include "include/theme_loader.hpp"
#include "include/ui/renderer.hpp"
#include "include/ui/theme.hpp"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <flux.h>
#include <iostream>

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

  // Stop notcurses completely to give terminal to external program
  if (g_nc) {
    notcurses_stop(g_nc);
    g_nc = nullptr;
    g_stdplane = nullptr;
  }
}

void resumeTerminal() {
  // Wait for terminal to settle after external program exits
  usleep(50000);

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
  if (!g_nc)
    return;

  g_stdplane = notcurses_stdplane(g_nc);

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
  fx::Renderer renderer(nc, stdplane); // ✨ Pass notcurses objects

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
    // std::cerr << "[fx] Loading theme: " << theme_name << " from " <<
    // *theme_path
    //           << "\n";
    auto def = fx::ThemeLoader::loadFromTOML(*theme_path);
    theme = theme_manager.applyThemeDefinition(def);
    g_theme = theme;

    // ADD THIS: Set the background for the entire stdplane
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
  fx::InputPrompt::setTheme(theme);
  renderer.setTheme(theme);
  renderer.setIconStyle(use_icons ? fx::IconStyle::AUTO : fx::IconStyle::ASCII);

  ncinput ni;
  // int ch;
  bool running = true;

  while (running) {
    browser.updateScroll(renderer.getViewportHeight());
    ncplane_erase(stdplane);
    renderer.render(browser);

    // Get input
    ncinput ni;
    memset(&ni, 0, sizeof(ni));
    uint32_t key = notcurses_get_blocking(nc, &ni);

    // DEBUG: Log what we're receiving

    // Handle error/interrupt
    if (key == (uint32_t)-1) {

      continue;
    }

    // CRITICAL: For special keys, ni.id is set and key might be 0
    // For regular characters, key contains the character and ni.id might be 0

    // Handle special keys first (these use ni.id)
    if (ni.id == NCKEY_RESIZE) {
      ncplane_erase(stdplane);
      browser.updateScroll(renderer.getViewportHeight());
      continue;
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
    }
    // Handle regular character keys (these use key, not ni.id)
    else if (key == 'k') {
      browser.selectPrevious();
    } else if (key == 'j') {
      browser.selectNext();
    } else if (key == 'h') {
      browser.navigateUp();
    } else if (key == 'l' || key == 10 || key == 13) { // l or Enter
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
      }
    } else if (key == 'g') {
      browser.selectFirst();
    } else if (key == 'G') {
      browser.selectLast();
    } else if (key == CTRL('b')) {
      browser.pageUp(renderer.getViewportHeight());
    } else if (key == CTRL('f')) {
      browser.pageDown(renderer.getViewportHeight());
    } else if (key == '.' || key == 'H') {
      browser.toggleHidden();
    } else if (key == 's') {
      browser.cycleSortMode();
    } else if (key == 'R') {
      browser.refresh();
    } else if (key == 'q' || key == CTRL('q') || key == 27) {
      running = false;
    } else if (key == 'a' || key == 'n') {
      auto name = fx::InputPrompt::getString(nc, stdplane, "New file: ");
      if (name) {
        if (browser.createFile(*name)) {
          // Success
        } else {
          const std::string selected_file = browser.getSelectedPath()->string();
          size_t saved_index = browser.getSelectedIndex();
          openFileWithHandler(selected_file, config);
          browser.selectByIndex(saved_index);
          browser.updateScroll(renderer.getViewportHeight());
          renderer.setTheme(g_theme);
        }
      }
    } else if (key == 'A' || key == 'N') {
      auto name = fx::InputPrompt::getString(nc, stdplane, "New directory: ");
      if (name) {
        if (browser.createDirectory(*name)) {
          // Success
        } else {
          std::string error = browser.getErrorMessage();
          unsigned width, height;
          ncplane_dim_yx(stdplane, &height, &width);
          ncplane_cursor_move_yx(stdplane, height - 1, 0);
        }
      }
    } else if (key == 'r') {
      const std::string default_name =
          browser.getEntryByIndex(browser.getSelectedIndex())->name;
      auto name =
          fx::InputPrompt::getString(nc, stdplane, "Rename: ", default_name);
      if (name) {
        if (!browser.renameEntry(browser.getSelectedIndex(), *name)) {
          // Handle error
        }
      }
    } else if (key == 'd') {
      if (fx::InputPrompt::getConfirmation(
              nc, stdplane, "Do you want to remove this file/folder? ")) {
        if (!browser.removeEntry(browser.getSelectedIndex())) {
          // Handle error
        }
      }
    }
  }

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