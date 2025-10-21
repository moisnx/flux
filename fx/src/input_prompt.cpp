#include "include/input_prompt.hpp"

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

namespace fx {

// Static member to hold theme reference
static flux::Theme *g_input_theme = nullptr;

void InputPrompt::setTheme(const flux::Theme &theme) {
  static flux::Theme stored_theme = theme;
  stored_theme = theme;
  g_input_theme = &stored_theme;
}

std::optional<std::string>
InputPrompt::getString(const std::string &prompt,
                       const std::string &default_value) {
  std::string input = default_value;
  size_t cursor_pos = input.length();

  // Save current screen state
  WINDOW *saved = dupwin(stdscr);

  // Enable cursor
  curs_set(1);

  while (true) {
    renderModal(prompt, input, cursor_pos);

    int ch = getch();

    switch (ch) {
    case 27: // Escape - cancel
      curs_set(0);
      // Restore screen
      overwrite(saved, stdscr);
      delwin(saved);
      refresh();
      return std::nullopt;

    case KEY_ENTER:
    case 10:
    case 13: // Enter - confirm
      curs_set(0);
      // Restore screen
      overwrite(saved, stdscr);
      delwin(saved);
      refresh();
      return input;

    case KEY_BACKSPACE:
    case 127:
    case 8: // Backspace
      if (cursor_pos > 0 && !input.empty()) {
        input.erase(cursor_pos - 1, 1);
        cursor_pos--;
      }
      break;

    case KEY_DC: // Delete key
      if (cursor_pos < input.length()) {
        input.erase(cursor_pos, 1);
      }
      break;

    case KEY_LEFT:
      if (cursor_pos > 0) {
        cursor_pos--;
      }
      break;

    case KEY_RIGHT:
      if (cursor_pos < input.length()) {
        cursor_pos++;
      }
      break;

    case KEY_HOME:
    case CTRL('a'):
      cursor_pos = 0;
      break;

    case KEY_END:
    case CTRL('e'):
      cursor_pos = input.length();
      break;

    case CTRL('u'): // Clear line
      input.clear();
      cursor_pos = 0;
      break;

    case CTRL('w'): // Delete word backward
      while (cursor_pos > 0 && input[cursor_pos - 1] == ' ') {
        input.erase(cursor_pos - 1, 1);
        cursor_pos--;
      }
      while (cursor_pos > 0 && input[cursor_pos - 1] != ' ') {
        input.erase(cursor_pos - 1, 1);
        cursor_pos--;
      }
      break;

    default:
      // Only allow printable characters
      if (ch >= 32 && ch < 127) {
        input.insert(cursor_pos, 1, static_cast<char>(ch));
        cursor_pos++;
      }
      break;
    }
  }
}

void InputPrompt::renderModal(const std::string &prompt,
                              const std::string &input, size_t cursor_pos) {
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  // Calculate modal dimensions
  int modal_width = std::min(60, max_x - 4);
  int modal_height = 7;
  int start_y = (max_y - modal_height) / 2;
  int start_x = (max_x - modal_width) / 2;

  // Create a window for the modal
  WINDOW *modal_win = newwin(modal_height, modal_width, start_y, start_x);

  // Apply theme colors if available
  if (g_input_theme && has_colors()) {
    wbkgd(modal_win, COLOR_PAIR(g_input_theme->status_bar));
  }

  // Draw border with theme colors
  if (g_input_theme && has_colors()) {
    wattron(modal_win, COLOR_PAIR(g_input_theme->ui_border));
  }

  box(modal_win, 0, 0);

  if (g_input_theme && has_colors()) {
    wattroff(modal_win, COLOR_PAIR(g_input_theme->ui_border));
  }

  // Draw title bar with accent color
  if (g_input_theme && has_colors()) {
    wattron(modal_win, COLOR_PAIR(g_input_theme->ui_accent) | A_BOLD);
  } else {
    wattron(modal_win, A_BOLD);
  }

  mvwprintw(modal_win, 1, 2, "%s", prompt.c_str());

  if (g_input_theme && has_colors()) {
    wattroff(modal_win, COLOR_PAIR(g_input_theme->ui_accent) | A_BOLD);
  } else {
    wattroff(modal_win, A_BOLD);
  }

  // Draw input box background
  int input_y = 3;
  int input_x = 2;
  int input_width = modal_width - 4;

  // Draw input box border
  if (g_input_theme && has_colors()) {
    wattron(modal_win, COLOR_PAIR(g_input_theme->ui_border));
  }

  mvwhline(modal_win, input_y, input_x, ACS_HLINE, input_width);
  mvwhline(modal_win, input_y + 2, input_x, ACS_HLINE, input_width);
  mvwvline(modal_win, input_y, input_x - 1, ACS_VLINE, 3);
  mvwvline(modal_win, input_y, input_x + input_width, ACS_VLINE, 3);
  mvwaddch(modal_win, input_y, input_x - 1, ACS_ULCORNER);
  mvwaddch(modal_win, input_y, input_x + input_width, ACS_URCORNER);
  mvwaddch(modal_win, input_y + 2, input_x - 1, ACS_LLCORNER);
  mvwaddch(modal_win, input_y + 2, input_x + input_width, ACS_LRCORNER);

  if (g_input_theme && has_colors()) {
    wattroff(modal_win, COLOR_PAIR(g_input_theme->ui_border));
  }

  // Draw input text
  if (g_input_theme && has_colors()) {
    wattron(modal_win, COLOR_PAIR(g_input_theme->foreground));
  }

  // Calculate visible portion of input if it's too long
  int visible_width = input_width - 2;
  size_t display_start = 0;

  if (cursor_pos >= visible_width) {
    display_start = cursor_pos - visible_width + 1;
  }

  std::string visible_input = input.substr(display_start, visible_width);
  mvwprintw(modal_win, input_y + 1, input_x + 1, "%s", visible_input.c_str());

  if (g_input_theme && has_colors()) {
    wattroff(modal_win, COLOR_PAIR(g_input_theme->foreground));
  }

  // Draw help text
  if (g_input_theme && has_colors()) {
    wattron(modal_win, COLOR_PAIR(g_input_theme->ui_secondary));
  }

  std::string help_text = "Enter to confirm • Esc to cancel";
  int help_x = (modal_width - static_cast<int>(help_text.size())) / 2;
  mvwprintw(modal_win, modal_height - 2, help_x, "%s", help_text.c_str());

  if (g_input_theme && has_colors()) {
    wattroff(modal_win, COLOR_PAIR(g_input_theme->ui_secondary));
  }

  // Position cursor in the modal
  int cursor_display_pos = cursor_pos - display_start;
  wmove(modal_win, input_y + 1, input_x + 1 + cursor_display_pos);

  // Refresh modal window
  wrefresh(modal_win);

  // Clean up
  delwin(modal_win);
}

bool InputPrompt::getConfirmation(const std::string &message) {
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  // Save current screen
  WINDOW *saved = dupwin(stdscr);

  // Calculate modal dimensions
  int modal_width = std::min(50, max_x - 4);
  int modal_height = 6;
  int start_y = (max_y - modal_height) / 2;
  int start_x = (max_x - modal_width) / 2;

  WINDOW *modal_win = newwin(modal_height, modal_width, start_y, start_x);

  // Apply theme colors
  if (g_input_theme && has_colors()) {
    wbkgd(modal_win, COLOR_PAIR(g_input_theme->status_bar));
    wattron(modal_win, COLOR_PAIR(g_input_theme->ui_border));
  }

  box(modal_win, 0, 0);

  if (g_input_theme && has_colors()) {
    wattroff(modal_win, COLOR_PAIR(g_input_theme->ui_border));
  }

  // Draw warning icon and message
  if (g_input_theme && has_colors()) {
    wattron(modal_win, COLOR_PAIR(g_input_theme->ui_warning) | A_BOLD);
  } else {
    wattron(modal_win, A_BOLD);
  }

  mvwprintw(modal_win, 1, 2, "⚠ Confirmation");

  if (g_input_theme && has_colors()) {
    wattroff(modal_win, COLOR_PAIR(g_input_theme->ui_warning) | A_BOLD);
  } else {
    wattroff(modal_win, A_BOLD);
  }

  // Message text
  if (g_input_theme && has_colors()) {
    wattron(modal_win, COLOR_PAIR(g_input_theme->foreground));
  }

  mvwprintw(modal_win, 3, 2, "%s", message.c_str());

  if (g_input_theme && has_colors()) {
    wattroff(modal_win, COLOR_PAIR(g_input_theme->foreground));
  }

  // Options
  if (g_input_theme && has_colors()) {
    wattron(modal_win, COLOR_PAIR(g_input_theme->ui_success));
  }
  mvwprintw(modal_win, modal_height - 2, 4, "[Y]es");
  if (g_input_theme && has_colors()) {
    wattroff(modal_win, COLOR_PAIR(g_input_theme->ui_success));
    wattron(modal_win, COLOR_PAIR(g_input_theme->ui_error));
  }
  mvwprintw(modal_win, modal_height - 2, modal_width - 12, "[N]o/Esc");
  if (g_input_theme && has_colors()) {
    wattroff(modal_win, COLOR_PAIR(g_input_theme->ui_error));
  }

  wrefresh(modal_win);

  bool result = false;
  while (true) {
    int ch = getch();

    switch (ch) {
    case 'y':
    case 'Y':
      result = true;
      goto cleanup;

    case 'n':
    case 'N':
    case 27: // Escape
      result = false;
      goto cleanup;
    }
  }

cleanup:
  delwin(modal_win);
  overwrite(saved, stdscr);
  delwin(saved);
  refresh();
  return result;
}

} // namespace fx