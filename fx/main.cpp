#include "include/theme_loader.h"
#include <cstdlib>
#include <flux.h>
#include <iostream>
#include <locale>
#ifdef _WIN32
#include <shellapi.h> // For ShellExecute
#include <windows.h>
#endif

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

void setup_terminal_attributes();
void restore_terminal_attributes();
void openFileWithDefaultApp(const std::string &filePath);
// Global variable to store old terminal attributes
#ifndef _WIN32
struct termios original_termios;
#endif

void printUsage(const char *program_name) {
  std::cout << "Flux - A modern terminal file browser\n\n";
  std::cout << "Usage: " << program_name << " [OPTIONS] [PATH]\n\n";
  std::cout << "Options:\n";
  std::cout << "  -h, --help       Show this help message\n";
  std::cout << "  -v, --version    Show version information\n";
  std::cout << "  --theme NAME     Use specified theme (default, gruvbox, "
               "nord, dracula)\n";
  std::cout << "  --no-icons       Disable icons (use ASCII only)\n";
  std::cout << "\n";
  std::cout << "Keybindings:\n";
  std::cout << "  j/k or ↑/↓       Navigate up/down\n";
  std::cout << "  h or ←           Go to parent directory\n";
  std::cout << "  l or → or Enter  Enter directory / open file\n";
  std::cout << "  g/G              Go to top/bottom\n";
  std::cout << "  .                Toggle hidden files\n";
  std::cout << "  s                Cycle sort mode\n";
  std::cout << "  q                Quit\n";
}

int main(int argc, char *argv[]) {
  // Parse command line arguments
  std::string start_path = ".";
  std::string theme_name = "dracula";
  bool use_icons = true;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else if (arg == "-v" || arg == "--version") {
      std::cout << "fx (Flux) version " << flux::getVersion() << "\n";
      return 0;
    } else if (arg == "--theme") {
      if (i + 1 < argc) {
        theme_name = argv[++i];
      } else {
        std::cerr << "Error: --theme requires an argument\n";
        return 1;
      }
    } else if (arg == "--no-icons") {
      use_icons = false;
    } else if (arg[0] != '-') {
      start_path = arg;
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
      return 1;
    }
  }

  // Set UTF-8 locale for proper icon rendering
  std::setlocale(LC_ALL, "");

  setup_terminal_attributes();

  // Initialize ncurses
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0); // Hide cursor

  if (has_colors()) {
    start_color();
    use_default_colors();
  }

  // Create browser and renderer
  flux::Browser browser(start_path);
  flux::Renderer renderer;

  // IMPORTANT: Create ThemeManager in STANDALONE mode (default)
  flux::ThemeManager theme_manager;
  // theme_manager.setEmbeddedMode(false); // This is the default

  flux::Theme theme;

  // Load theme from file
  auto theme_path = fx::ThemeLoader::findThemeFile(theme_name);
  if (theme_path) {
    std::cerr << "Loading theme from: " << *theme_path << "\n";
    auto def = fx::ThemeLoader::loadFromTOML(*theme_path);
    std::cerr << "Background: " << def.background << std::endl;

    // Apply theme - this WILL call init_pair() in standalone mode
    theme = theme_manager.applyThemeDefinition(def);

    // Set background if theme specifies one
    if (def.background != "transparent" && def.background != "default" &&
        !def.background.empty()) {
      bkgd(COLOR_PAIR(theme.background));
    }
  } else {
    std::cerr << "Theme '" << theme_name << "' not found, using default\n";
    theme = theme_manager.applyThemeDefinition(
        flux::ThemeManager::getDefaultThemeDef());
    bkgd(COLOR_PAIR(theme.background));
  }

  renderer.setTheme(theme);

  // Set icon style
  if (use_icons) {
    renderer.setIconStyle(IconStyle::AUTO);
  } else {
    renderer.setIconStyle(IconStyle::ASCII);
  }

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
      break;

    case KEY_RIGHT:
    case KEY_ENTER:
    case 10:
    case 13: {
      if (browser.isSelectedDirectory()) {
        browser.navigateInto(browser.getSelectedIndex());
      } else {
        const std::string selected_file = browser.getSelectedPath()->string();
        openFileWithDefaultApp(selected_file);
        break;
      }
      break;
    }
    }
  }

  endwin();
  restore_terminal_attributes();
  return 0;
}

void setup_terminal_attributes() {
#ifndef _WIN32
  // Get current terminal settings and store them
  tcgetattr(STDIN_FILENO, &original_termios);
  struct termios new_termios = original_termios;

  // Disable software flow control (IXON)
  new_termios.c_iflag &= ~IXON;

  // Apply the new settings
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
#endif
}

void restore_terminal_attributes() {
#ifndef _WIN32
  // Restore the original terminal settings
  tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
#endif
}

void openFileWithDefaultApp(const std::string &filePath) {
#ifdef _WIN32
  // 1. Restore terminal state BEFORE launching the external app
  endwin();                      // Close ncurses screen mode
  restore_terminal_attributes(); // Restore custom terminal settings (though
                                 // ncurses endwin() should cover most)

  SHELLEXECUTEINFOA sei = {sizeof(sei)};
  sei.lpFile = filePath.c_str();
  sei.nShow = SW_SHOWNORMAL;
  sei.lpVerb = "open";
  sei.fMask =
      SEE_MASK_NOCLOSEPROCESS |
      SEE_MASK_FLAG_NO_UI; // Crucial: get process handle and suppress errors

  if (ShellExecuteExA(&sei)) {
    if (sei.hProcess) {
      // 2. Wait for the external process to close
      WaitForSingleObject(sei.hProcess, INFINITE);
      CloseHandle(sei.hProcess);
    }
  } else {
    std::cerr << "Error launching file on Windows." << std::endl;
  }

  // 3. Reinitialize ncurses and refresh the screen
  refresh(); // Re-enters ncurses mode
  // You may need to call the ncurses setup functions again, or at least
  // doupdate() A simple 'refresh()' will re-enter ncurses mode and redraw the
  // screen, but often a full redraw with 'doupdate()' or 'redrawwin(stdscr);
  // doupdate();' is safer.
  redrawwin(stdscr);
  doupdate();

#elif __APPLE__ || __linux__
  // Unix/Linux/macOS (using std::system)
  // std::system() already waits for the command to finish, so we only need the
  // suspend/resume

  // 1. Restore terminal state BEFORE launching the external app
  endwin();
  restore_terminal_attributes();

  std::string command;
#ifdef __APPLE__
  command = "open \"" + filePath + "\"";
#else
  command = "xdg-open \"" + filePath + "\"";
#endif

  int result = std::system(command.c_str());

  if (result != 0) {
    std::cerr << "Error opening file. Command: " << command << std::endl;
  }

  // 2. Reinitialize ncurses and refresh the screen
  // The main loop will call refresh() implicitly, but you must call initscr()
  // again
  initscr(); // Re-enters ncurses mode
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0); // Hide cursor

  // Re-run color setup if necessary, and re-apply the background
  if (has_colors()) {
    start_color();
    use_default_colors();
  }

  // The theme/color pairs should still be initialized, but you might need to
  // re-apply the background: (This part depends on your flux::ThemeManager
  // implementation, but for simplicity, we'll assume the main loop will take
  // care of the full render shortly)

  redrawwin(stdscr);
  doupdate();

#else
  // Other/Unknown OS
  std::cerr << "File opening not supported for this operating system."
            << std::endl;
#endif
}