// include/input_prompt.hpp
#pragma once

#include "ui/theme.hpp"
#include <notcurses/notcurses.h>
#include <optional>
#include <string>

namespace fx {

class InputPrompt {
public:
  static void setTheme(const Theme &theme);

  static std::optional<std::string>
  getString(notcurses *nc, ncplane *stdplane, const std::string &prompt,
            const std::string &default_value = "");

  static bool getConfirmation(notcurses *nc, ncplane *stdplane,
                              const std::string &message);

private:
  static void renderModal(ncplane *modal_plane, const std::string &prompt,
                          const std::string &input, size_t cursor_pos);

  static void renderConfirmationModal(ncplane *modal_plane,
                                      const std::string &message);
};

} // namespace fx