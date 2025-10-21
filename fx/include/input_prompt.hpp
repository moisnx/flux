#include <flux.h>

namespace fx {

class InputPrompt {
public:
  // Set theme for modal rendering
  static void setTheme(const flux::Theme &theme);

  // Get string input from user with a modal prompt
  static std::optional<std::string>
  getString(const std::string &prompt, const std::string &default_value = "");

  // Get confirmation (y/n) with a modal prompt
  static bool getConfirmation(const std::string &message);

private:
  static void renderModal(const std::string &prompt, const std::string &input,
                          size_t cursor_pos);
};

} // namespace fx