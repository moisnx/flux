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

std::optional<std::string>
InputPrompt::getString(const std::string &prompt,
                       const std::string &default_value) {
  std::string input = default_value;
  size_t cursor_pos = input.length();

  // Enable echo temporarily for better UX
  echo();
  curs_set(1); // Show cursor

  while (true) {
    renderPrompt(prompt, input, cursor_pos);

    int ch = getch();

    switch (ch) {
    case 27: // Escape - cancel
      noecho();
      curs_set(0);
      return std::nullopt;

    case KEY_ENTER:
    case 10:
    case 13: // Enter - confirm
      noecho();
      curs_set(0);
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

void InputPrompt::renderPrompt(const std::string &prompt,
                               const std::string &input, size_t cursor_pos) {
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  // Clear bottom line
  move(max_y - 1, 0);
  clrtoeol();

  // Draw prompt
  attron(A_BOLD);
  mvprintw(max_y - 1, 0, "%s", prompt.c_str());
  attroff(A_BOLD);

  // Draw input
  int input_start = prompt.length();
  mvprintw(max_y - 1, input_start, "%s", input.c_str());

  // Position cursor
  move(max_y - 1, input_start + cursor_pos);

  refresh();
}

bool InputPrompt::getConfirmation(const std::string &message) {
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  // Draw message
  move(max_y - 1, 0);
  clrtoeol();
  attron(A_BOLD);
  mvprintw(max_y - 1, 0, "%s (y/n): ", message.c_str());
  attroff(A_BOLD);
  refresh();

  while (true) {
    int ch = getch();

    switch (ch) {
    case 'y':
    case 'Y':
      return true;

    case 'n':
    case 'N':
    case 27: // Escape
      return false;
    }
  }
}

} // namespace fx