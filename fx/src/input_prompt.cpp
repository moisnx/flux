#include "include/input_prompt.hpp"
#include <notcurses/notcurses.h>

#ifndef CTRL
#define CTRL(c) ((c) & 0x1f)
#endif

namespace fx {

static Theme *g_input_theme = nullptr;

void InputPrompt::setTheme(const Theme &theme) {
  static Theme stored_theme = theme;
  stored_theme = theme;
  g_input_theme = &stored_theme;
}

std::optional<std::string>
InputPrompt::getString(notcurses *nc, ncplane *stdplane,
                       const std::string &prompt,
                       const std::string &default_value) {
  std::string input = default_value;
  size_t cursor_pos = input.length();

  // Get screen dimensions
  unsigned max_y, max_x;
  ncplane_dim_yx(stdplane, &max_y, &max_x);

  // Calculate modal dimensions
  int modal_width = std::min(60, static_cast<int>(max_x) - 4);
  int modal_height = 7;
  int start_y = (static_cast<int>(max_y) - modal_height) / 2;
  int start_x = (static_cast<int>(max_x) - modal_width) / 2;

  // Create modal plane
  struct ncplane_options nopts = {
      .y = start_y,
      .x = start_x,
      .rows = static_cast<unsigned>(modal_height),
      .cols = static_cast<unsigned>(modal_width),
      .userptr = nullptr,
      .name = "input_modal",
      .resizecb = nullptr,
      .flags = 0,
      .margin_b = 0,
      .margin_r = 0,
  };

  ncplane *modal_plane = ncplane_create(stdplane, &nopts);
  if (!modal_plane) {
    return std::nullopt;
  }

  // Show cursor
  notcurses_cursor_enable(nc, start_y + 4, start_x + 3);

  ncinput ni;
  while (true) {
    renderModal(modal_plane, prompt, input, cursor_pos);
    notcurses_render(nc);

    uint32_t key = notcurses_get_blocking(nc, &ni);

    if (key == 27) { // Escape - cancel
      notcurses_cursor_disable(nc);
      ncplane_destroy(modal_plane);
      ncplane_erase(stdplane);
      notcurses_render(nc);
      return std::nullopt;
    } else if (ni.id == NCKEY_ENTER || key == 10 ||
               key == 13) { // Enter - confirm
      notcurses_cursor_disable(nc);
      ncplane_destroy(modal_plane);
      ncplane_erase(stdplane);
      notcurses_render(nc);
      return input;
    } else if (ni.id == NCKEY_BACKSPACE || key == 127 ||
               key == 8) { // Backspace
      if (cursor_pos > 0 && !input.empty()) {
        input.erase(cursor_pos - 1, 1);
        cursor_pos--;
      }
    } else if (ni.id == NCKEY_DEL) { // Delete
      if (cursor_pos < input.length()) {
        input.erase(cursor_pos, 1);
      }
    } else if (ni.id == NCKEY_LEFT) {
      if (cursor_pos > 0) {
        cursor_pos--;
      }
    } else if (ni.id == NCKEY_RIGHT) {
      if (cursor_pos < input.length()) {
        cursor_pos++;
      }
    } else if (ni.id == NCKEY_HOME || key == CTRL('a')) {
      cursor_pos = 0;
    } else if (ni.id == NCKEY_END || key == CTRL('e')) {
      cursor_pos = input.length();
    } else if (key == CTRL('u')) { // Clear line
      input.clear();
      cursor_pos = 0;
    } else if (key == CTRL('w')) { // Delete word backward
      while (cursor_pos > 0 && input[cursor_pos - 1] == ' ') {
        input.erase(cursor_pos - 1, 1);
        cursor_pos--;
      }
      while (cursor_pos > 0 && input[cursor_pos - 1] != ' ') {
        input.erase(cursor_pos - 1, 1);
        cursor_pos--;
      }
    } else if (key >= 32 && key < 127) { // Printable characters
      input.insert(cursor_pos, 1, static_cast<char>(key));
      cursor_pos++;
    }

    // Update cursor position
    int display_start = 0;
    int visible_width = modal_width - 6;
    if (static_cast<int>(cursor_pos) >= visible_width) {
      display_start = cursor_pos - visible_width + 1;
    }
    int cursor_display_pos = cursor_pos - display_start;
    notcurses_cursor_enable(nc, start_y + 4, start_x + 3 + cursor_display_pos);
  }
}

void InputPrompt::renderModal(ncplane *modal_plane, const std::string &prompt,
                              const std::string &input, size_t cursor_pos) {
  unsigned modal_height, modal_width;
  ncplane_dim_yx(modal_plane, &modal_height, &modal_width);

  ncplane_erase(modal_plane);

  // Set background
  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels, (g_input_theme->foreground >> 16) & 0xFF,
                           (g_input_theme->foreground >> 8) & 0xFF,
                           g_input_theme->foreground & 0xFF);
    ncchannels_set_bg_rgb8(&channels, (g_input_theme->status_bar >> 16) & 0xFF,
                           (g_input_theme->status_bar >> 8) & 0xFF,
                           g_input_theme->status_bar & 0xFF);
    ncplane_set_channels(modal_plane, channels);
  }

  // Draw border with Unicode box drawing
  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels, (g_input_theme->ui_border >> 16) & 0xFF,
                           (g_input_theme->ui_border >> 8) & 0xFF,
                           g_input_theme->ui_border & 0xFF);
    ncplane_set_channels(modal_plane, channels);
  }

  // Draw box manually with Unicode
  ncplane_putstr_yx(modal_plane, 0, 0, "┌");
  for (unsigned i = 1; i < modal_width - 1; ++i) {
    ncplane_putstr(modal_plane, "─");
  }
  ncplane_putstr(modal_plane, "┐");

  for (unsigned i = 1; i < modal_height - 1; ++i) {
    ncplane_putstr_yx(modal_plane, i, 0, "│");
    ncplane_putstr_yx(modal_plane, i, modal_width - 1, "│");
  }

  ncplane_putstr_yx(modal_plane, modal_height - 1, 0, "└");
  for (unsigned i = 1; i < modal_width - 1; ++i) {
    ncplane_putstr(modal_plane, "─");
  }
  ncplane_putstr(modal_plane, "┘");

  // Draw prompt with accent color
  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels, (g_input_theme->ui_accent >> 16) & 0xFF,
                           (g_input_theme->ui_accent >> 8) & 0xFF,
                           g_input_theme->ui_accent & 0xFF);
    ncplane_set_channels(modal_plane, channels);
    ncplane_set_styles(modal_plane, NCSTYLE_BOLD);
  }

  ncplane_putstr_yx(modal_plane, 1, 2, prompt.c_str());
  ncplane_set_styles(modal_plane, NCSTYLE_NONE);

  // Draw input box border
  int input_y = 3;
  int input_x = 2;
  int input_width = modal_width - 4;

  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels, (g_input_theme->ui_border >> 16) & 0xFF,
                           (g_input_theme->ui_border >> 8) & 0xFF,
                           g_input_theme->ui_border & 0xFF);
    ncplane_set_channels(modal_plane, channels);
  }

  // Draw input box with Unicode
  ncplane_putstr_yx(modal_plane, input_y, input_x - 1, "┌");
  for (int i = 0; i < input_width; ++i) {
    ncplane_putstr(modal_plane, "─");
  }
  ncplane_putstr(modal_plane, "┐");

  ncplane_putstr_yx(modal_plane, input_y + 1, input_x - 1, "│");
  ncplane_putstr_yx(modal_plane, input_y + 1, input_x + input_width, "│");

  ncplane_putstr_yx(modal_plane, input_y + 2, input_x - 1, "└");
  for (int i = 0; i < input_width; ++i) {
    ncplane_putstr(modal_plane, "─");
  }
  ncplane_putstr(modal_plane, "┘");

  // Draw input text
  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels, (g_input_theme->foreground >> 16) & 0xFF,
                           (g_input_theme->foreground >> 8) & 0xFF,
                           g_input_theme->foreground & 0xFF);
    ncplane_set_channels(modal_plane, channels);
  }

  // Calculate visible portion
  int visible_width = input_width - 2;
  size_t display_start = 0;
  if (static_cast<int>(cursor_pos) >= visible_width) {
    display_start = cursor_pos - visible_width + 1;
  }

  std::string visible_input = input.substr(display_start, visible_width);
  ncplane_putstr_yx(modal_plane, input_y + 1, input_x + 1,
                    visible_input.c_str());

  // Draw help text
  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels,
                           (g_input_theme->ui_secondary >> 16) & 0xFF,
                           (g_input_theme->ui_secondary >> 8) & 0xFF,
                           g_input_theme->ui_secondary & 0xFF);
    ncplane_set_channels(modal_plane, channels);
  }

  std::string help_text = "Enter to confirm • Esc to cancel";
  int help_x = (modal_width - static_cast<int>(help_text.size())) / 2;
  ncplane_putstr_yx(modal_plane, modal_height - 2, help_x, help_text.c_str());
}

bool InputPrompt::getConfirmation(notcurses *nc, ncplane *stdplane,
                                  const std::string &message) {
  unsigned max_y, max_x;
  ncplane_dim_yx(stdplane, &max_y, &max_x);

  // Calculate modal dimensions
  int modal_width = std::min(50, static_cast<int>(max_x) - 4);
  int modal_height = 6;
  int start_y = (static_cast<int>(max_y) - modal_height) / 2;
  int start_x = (static_cast<int>(max_x) - modal_width) / 2;

  // Create modal plane
  struct ncplane_options nopts = {
      .y = start_y,
      .x = start_x,
      .rows = static_cast<unsigned>(modal_height),
      .cols = static_cast<unsigned>(modal_width),
      .userptr = nullptr,
      .name = "confirm_modal",
      .resizecb = nullptr,
      .flags = 0,
      .margin_b = 0,
      .margin_r = 0,
  };

  ncplane *modal_plane = ncplane_create(stdplane, &nopts);
  if (!modal_plane) {
    return false;
  }

  renderConfirmationModal(modal_plane, message);
  notcurses_render(nc);

  bool result = false;
  ncinput ni;

  while (true) {
    uint32_t key = notcurses_get_blocking(nc, &ni);

    if (key == 'y' || key == 'Y') {
      result = true;
      break;
    } else if (key == 'n' || key == 'N' || key == 27) { // Escape
      result = false;
      break;
    }
  }

  ncplane_destroy(modal_plane);
  ncplane_erase(stdplane);
  notcurses_render(nc);
  return result;
}

void InputPrompt::renderConfirmationModal(ncplane *modal_plane,
                                          const std::string &message) {
  unsigned modal_height, modal_width;
  ncplane_dim_yx(modal_plane, &modal_height, &modal_width);

  ncplane_erase(modal_plane);

  // Set background
  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels, (g_input_theme->foreground >> 16) & 0xFF,
                           (g_input_theme->foreground >> 8) & 0xFF,
                           g_input_theme->foreground & 0xFF);
    ncchannels_set_bg_rgb8(&channels, (g_input_theme->status_bar >> 16) & 0xFF,
                           (g_input_theme->status_bar >> 8) & 0xFF,
                           g_input_theme->status_bar & 0xFF);
    ncplane_set_channels(modal_plane, channels);
  }

  // Draw border
  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels, (g_input_theme->ui_border >> 16) & 0xFF,
                           (g_input_theme->ui_border >> 8) & 0xFF,
                           g_input_theme->ui_border & 0xFF);
    ncplane_set_channels(modal_plane, channels);
  }

  // Draw box with Unicode
  ncplane_putstr_yx(modal_plane, 0, 0, "┌");
  for (unsigned i = 1; i < modal_width - 1; ++i) {
    ncplane_putstr(modal_plane, "─");
  }
  ncplane_putstr(modal_plane, "┐");

  for (unsigned i = 1; i < modal_height - 1; ++i) {
    ncplane_putstr_yx(modal_plane, i, 0, "│");
    ncplane_putstr_yx(modal_plane, i, modal_width - 1, "│");
  }

  ncplane_putstr_yx(modal_plane, modal_height - 1, 0, "└");
  for (unsigned i = 1; i < modal_width - 1; ++i) {
    ncplane_putstr(modal_plane, "─");
  }
  ncplane_putstr(modal_plane, "┘");

  // Draw warning title
  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels, (g_input_theme->ui_warning >> 16) & 0xFF,
                           (g_input_theme->ui_warning >> 8) & 0xFF,
                           g_input_theme->ui_warning & 0xFF);
    ncplane_set_channels(modal_plane, channels);
    ncplane_set_styles(modal_plane, NCSTYLE_BOLD);
  }

  ncplane_putstr_yx(modal_plane, 1, 2, "⚠ Confirmation");
  ncplane_set_styles(modal_plane, NCSTYLE_NONE);

  // Message text
  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels, (g_input_theme->foreground >> 16) & 0xFF,
                           (g_input_theme->foreground >> 8) & 0xFF,
                           g_input_theme->foreground & 0xFF);
    ncplane_set_channels(modal_plane, channels);
  }

  ncplane_putstr_yx(modal_plane, 3, 2, message.c_str());

  // Options
  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels, (g_input_theme->ui_success >> 16) & 0xFF,
                           (g_input_theme->ui_success >> 8) & 0xFF,
                           g_input_theme->ui_success & 0xFF);
    ncplane_set_channels(modal_plane, channels);
  }
  ncplane_putstr_yx(modal_plane, modal_height - 2, 4, "[Y]es");

  if (g_input_theme) {
    uint64_t channels = 0;
    ncchannels_set_fg_rgb8(&channels, (g_input_theme->ui_error >> 16) & 0xFF,
                           (g_input_theme->ui_error >> 8) & 0xFF,
                           g_input_theme->ui_error & 0xFF);
    ncplane_set_channels(modal_plane, channels);
  }
  ncplane_putstr_yx(modal_plane, modal_height - 2, modal_width - 12,
                    "[N]o/Esc");
}

} // namespace fx