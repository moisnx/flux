// Standalone file browser binary

#include "theme_loader.h"
#include <cstdlib>
#include <flux.h>
#include <iostream>
#include <locale>

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
  std::string theme_name = "dracula"; // Remove .toml extension
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

  // Initialize ncurses AFTER loading theme
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

  flux::ThemeManager theme_manager;
  flux::Theme theme;

  auto theme_path = ThemeLoader::findThemeFile(theme_name);
  if (theme_path) {
    std::cerr << "Loading theme from: " << *theme_path << "\n";
    auto def = ThemeLoader::loadFromTOML(*theme_path);
    std::cerr << def.background << std::endl;
    theme = theme_manager.applyThemeDefinition(def);
    if (def.background != "transparent" && def.background != "default" &&
        !def.background.empty()) {
      bkgd(COLOR_PAIR(theme.background));
    }
  } else {
    std::cerr << "Theme '" << theme_name << "' not found, using default\n";
    theme = theme_manager.applyThemeDefinition(
        flux::ThemeManager::getDefaultThemeDef());
  }

  renderer.setTheme(theme);
  bkgd(COLOR_PAIR(theme.background));

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
        break;
      }
      break;
    }
    }
  }

  endwin();
  restore_terminal_attributes();
  clear();
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