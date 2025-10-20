#ifndef FX_FILE_OPENER_HPP
#define FX_FILE_OPENER_HPP

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace fx {

class FileOpener {
public:
  struct OpenConfig {
    std::string command;
    bool wait_for_completion = true;
    bool validate_command = false;
    std::optional<std::filesystem::path> allowed_base_dir = std::nullopt;
  };

  struct OpenResult {
    bool success;
    std::string error_message;
  };

  // Callback type for terminal suspend/resume
  using TerminalCallback = std::function<void()>;

  // Set callbacks for terminal state management
  static void setTerminalSuspendCallback(TerminalCallback callback) {
    suspend_callback_ = callback;
  }

  static void setTerminalResumeCallback(TerminalCallback callback) {
    resume_callback_ = callback;
  }

  // Open file with specific application
  static OpenResult openWith(const std::string &file_path,
                             const OpenConfig &config);

  // Open with system default handler
  static OpenResult openWithDefault(const std::string &file_path);

  // Whitelist management
  static void addAllowedCommand(const std::string &command);
  static void clearAllowedCommands();
  static void enableWhitelist(bool enable) { use_whitelist_ = enable; }
  static bool isWhitelistEnabled() { return use_whitelist_; }

private:
  static std::vector<std::string> allowed_commands_;
  static bool use_whitelist_;
  static TerminalCallback suspend_callback_;
  static TerminalCallback resume_callback_;

  static std::vector<std::string> parseCommand(const std::string &command);
  static std::optional<std::filesystem::path> validatePath(
      const std::string &path,
      const std::optional<std::filesystem::path> &base_dir = std::nullopt);
  static bool isCommandAllowed(const std::string &command);

#ifndef _WIN32
  static bool executeUnix(const std::vector<std::string> &parts,
                          const std::filesystem::path &file_path, bool wait);
#endif

#ifdef _WIN32
  static bool executeWindows(const std::string &command,
                             const std::filesystem::path &file_path, bool wait);
#endif
};

} // namespace fx

#endif // FX_FILE_OPENER_HPP