#ifndef FX_FILE_OPENER_HPP
#define FX_FILE_OPENER_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fx {

/**
 * @brief Secure file opener that prevents command injection attacks
 *
 * This module provides safe file opening functionality by using OS-level
 * APIs that don't invoke a shell, preventing command injection through
 * malicious filenames or handler commands.
 */
class FileOpener {
public:
  /**
   * @brief Result of a file opening operation
   */
  struct OpenResult {
    bool success;
    std::string error_message;

    explicit operator bool() const { return success; }
  };

  /**
   * @brief Configuration for opening a file
   */
  struct OpenConfig {
    std::string command;      // Command to execute
    bool wait_for_completion; // Wait for process to finish
    bool validate_command;    // Check if command is in whitelist
    std::optional<std::filesystem::path>
        allowed_base_dir; // Restrict to directory
  };

  /**
   * @brief Open a file with a specific command (safe, no shell)
   *
   * Uses fork+execvp on Unix or CreateProcess on Windows to avoid
   * shell interpretation and prevent command injection.
   *
   * @param file_path Path to file to open
   * @param config Configuration for opening
   * @return Result indicating success/failure
   */
  static OpenResult openWith(const std::string &file_path,
                             const OpenConfig &config);

  /**
   * @brief Open a file with the system's default handler
   *
   * Uses xdg-open (Linux), open (macOS), or ShellExecute (Windows)
   *
   * @param file_path Path to file to open
   * @return Result indicating success/failure
   */
  static OpenResult openWithDefault(const std::string &file_path);

  /**
   * @brief Check if a command is in the allowed whitelist
   *
   * @param command Command to check
   * @return true if allowed, false otherwise
   */
  static bool isCommandAllowed(const std::string &command);

  /**
   * @brief Add a command to the whitelist
   *
   * @param command Command executable name (e.g., "vim", "code")
   */
  static void addAllowedCommand(const std::string &command);

  /**
   * @brief Clear the command whitelist
   */
  static void clearAllowedCommands();

  /**
   * @brief Validate and canonicalize a file path
   *
   * Ensures the path exists, is accessible, and optionally
   * is within an allowed base directory.
   *
   * @param path Path to validate
   * @param base_dir Optional base directory to restrict to
   * @return Canonical path if valid, nullopt otherwise
   */
  static std::optional<std::filesystem::path> validatePath(
      const std::string &path,
      const std::optional<std::filesystem::path> &base_dir = std::nullopt);

private:
  /**
   * @brief Parse a command string into program and arguments
   *
   * Handles quoted arguments properly.
   *
   * @param command Command string to parse
   * @return Vector of command parts [program, arg1, arg2, ...]
   */
  static std::vector<std::string> parseCommand(const std::string &command);

  /**
   * @brief Execute a command safely without shell on Unix
   *
   * @param parts Command parts from parseCommand
   * @param file_path File to pass as argument
   * @param wait Wait for completion
   * @return true if successful
   */
#ifndef _WIN32
  static bool executeUnix(const std::vector<std::string> &parts,
                          const std::filesystem::path &file_path, bool wait);
#endif

  /**
   * @brief Execute a command on Windows
   *
   * @param command Command to execute
   * @param file_path File to pass as argument
   * @param wait Wait for completion
   * @return true if successful
   */
#ifdef _WIN32
  static bool executeWindows(const std::string &command,
                             const std::filesystem::path &file_path, bool wait);
#endif

  // Command whitelist
  static std::vector<std::string> allowed_commands_;
  static bool use_whitelist_;
};

} // namespace fx

#endif // FX_FILE_OPENER_HPP