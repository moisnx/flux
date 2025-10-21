#ifndef INPUT_PROMPT_HPP
#define INPUT_PROMPT_HPP

#include <optional>
#include <string>

namespace fx {

class InputPrompt {
public:
  // Prompt user for a string input
  // Returns std::nullopt if cancelled (Esc pressed)
  static std::optional<std::string>
  getString(const std::string &prompt, const std::string &default_value = "");

  // Prompt user for yes/no confirmation
  // Returns true for 'y', false for 'n' or Esc
  static bool getConfirmation(const std::string &message);

private:
  static void renderPrompt(const std::string &prompt, const std::string &input,
                           size_t cursor_pos);
};

} // namespace fx

#endif