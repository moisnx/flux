#include "include/file_opener.hpp"
#include <algorithm>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fx {

// Static member initialization
std::vector<std::string> FileOpener::allowed_commands_ = {
    // Text editors
    "arc",
    "vim",
    "nvim",
    "vi",
    "nano",
    "emacs",
    "emacsclient",
    "code",
    "subl",
    "atom",
    "gedit",
    "kate",
    "kwrite",
    "notepad",
    "notepad++",

    // File viewers
    "less",
    "more",
    "cat",
    "bat",
    "most",

    // Image viewers
    "feh",
    "sxiv",
    "eog",
    "eom",
    "gwenview",
    "gthumb",
    "gimp",
    "krita",
    "inkscape",

    // Video/Audio players
    "mpv",
    "vlc",
    "mplayer",
    "ffplay",
    "totem",

    // PDF viewers
    "zathura",
    "evince",
    "okular",
    "mupdf",
    "xpdf",

    // Browsers
    "firefox",
    "chrome",
    "chromium",
    "brave",
    "safari",

    // Archive managers
    "file-roller",
    "ark",
    "xarchiver",
};

bool FileOpener::use_whitelist_ = false; // Disabled by default for flexibility

std::vector<std::string> FileOpener::parseCommand(const std::string &command) {
  std::vector<std::string> parts;
  std::string current;
  bool in_quotes = false;
  char quote_char = '\0';

  for (size_t i = 0; i < command.size(); ++i) {
    char c = command[i];

    if ((c == '"' || c == '\'') && !in_quotes) {
      in_quotes = true;
      quote_char = c;
    } else if (c == quote_char && in_quotes) {
      in_quotes = false;
      quote_char = '\0';
    } else if (c == ' ' && !in_quotes) {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }

  if (!current.empty()) {
    parts.push_back(current);
  }

  return parts;
}

std::optional<std::filesystem::path>
FileOpener::validatePath(const std::string &path,
                         const std::optional<std::filesystem::path> &base_dir) {
  try {
    // Check if path exists
    if (!std::filesystem::exists(path)) {
      return std::nullopt;
    }

    // Get canonical (absolute, symlinks resolved) path
    auto canonical = std::filesystem::canonical(path);

    // If base directory specified, ensure file is within it
    if (base_dir) {
      auto base_canonical = std::filesystem::canonical(*base_dir);
      auto relative = std::filesystem::relative(canonical, base_canonical);

      // Check if path tries to escape base directory
      std::string rel_str = relative.string();
      if (rel_str.size() >= 2 && rel_str.substr(0, 2) == "..") {
        return std::nullopt;
      }
    }

    return canonical;

  } catch (const std::filesystem::filesystem_error &e) {
    return std::nullopt;
  }
}

bool FileOpener::isCommandAllowed(const std::string &command) {
  if (!use_whitelist_) {
    return true; // Whitelist disabled
  }

  // Extract executable name from command
  auto parts = parseCommand(command);
  if (parts.empty()) {
    return false;
  }

  std::string exe = parts[0];

  // Remove path, keep just basename
  size_t slash_pos = exe.find_last_of("/\\");
  if (slash_pos != std::string::npos) {
    exe = exe.substr(slash_pos + 1);
  }

  // Remove .exe extension on Windows
#ifdef _WIN32
  if (exe.size() > 4 && exe.substr(exe.size() - 4) == ".exe") {
    exe = exe.substr(0, exe.size() - 4);
  }
#endif

  // Check if in whitelist
  return std::find(allowed_commands_.begin(), allowed_commands_.end(), exe) !=
         allowed_commands_.end();
}

void FileOpener::addAllowedCommand(const std::string &command) {
  if (std::find(allowed_commands_.begin(), allowed_commands_.end(), command) ==
      allowed_commands_.end()) {
    allowed_commands_.push_back(command);
  }
}

void FileOpener::clearAllowedCommands() { allowed_commands_.clear(); }

#ifndef _WIN32
bool FileOpener::executeUnix(const std::vector<std::string> &parts,
                             const std::filesystem::path &file_path,
                             bool wait) {
  pid_t pid = fork();

  if (pid < 0) {
    return false; // Fork failed
  }

  if (pid == 0) {
    // Child process

    // Build argument array for execvp
    std::vector<const char *> args;

    // Add program name
    args.push_back(parts[0].c_str());

    // Add original arguments from command (skip program name)
    for (size_t i = 1; i < parts.size(); ++i) {
      args.push_back(parts[i].c_str());
    }

    // Add file path as last argument
    std::string path_str = file_path.string();
    args.push_back(path_str.c_str());
    args.push_back(nullptr); // execvp requires null terminator

    // Execute - this replaces the child process
    // NO SHELL is invoked, so no command injection possible!
    execvp(args[0], const_cast<char *const *>(args.data()));

    // If execvp returns, it failed
    std::cerr << "Failed to execute: " << args[0] << std::endl;
    _exit(127);
  }

  // Parent process
  if (wait) {
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
  }

  // For GUI apps, don't wait
  return true;
}
#endif

#ifdef _WIN32
bool FileOpener::executeWindows(const std::string &command,
                                const std::filesystem::path &file_path,
                                bool wait) {
  // Parse command to get executable and arguments
  auto parts = parseCommand(command);
  if (parts.empty()) {
    return false;
  }

  std::string exe = parts[0];
  std::string args;

  // Build argument string
  for (size_t i = 1; i < parts.size(); ++i) {
    if (!args.empty())
      args += " ";
    args += "\"" + parts[i] + "\"";
  }

  // Add file path
  if (!args.empty())
    args += " ";
  args += "\"" + file_path.string() + "\"";

  SHELLEXECUTEINFOA sei = {sizeof(sei)};
  sei.lpFile = exe.c_str();
  sei.lpParameters = args.empty() ? nullptr : args.c_str();
  sei.nShow = SW_SHOWNORMAL;
  sei.fMask = wait ? SEE_MASK_NOCLOSEPROCESS : 0;

  if (!ShellExecuteExA(&sei)) {
    return false;
  }

  if (wait && sei.hProcess) {
    WaitForSingleObject(sei.hProcess, INFINITE);
    CloseHandle(sei.hProcess);
  }

  return true;
}
#endif

FileOpener::OpenResult FileOpener::openWith(const std::string &file_path,
                                            const OpenConfig &config) {
  // Validate file path
  auto canonical_path = validatePath(file_path, config.allowed_base_dir);
  if (!canonical_path) {
    return OpenResult{false, "Invalid or inaccessible file path"};
  }

  // Check command whitelist if enabled
  if (config.validate_command && !isCommandAllowed(config.command)) {
    return OpenResult{false,
                      "Command not in allowed whitelist: " + config.command};
  }

  // Parse command
  auto parts = parseCommand(config.command);
  if (parts.empty()) {
    return OpenResult{false, "Empty command"};
  }

  // Execute based on platform
  bool success = false;

#ifdef _WIN32
  success = executeWindows(config.command, *canonical_path,
                           config.wait_for_completion);
#else
  success = executeUnix(parts, *canonical_path, config.wait_for_completion);
#endif

  if (!success) {
    return OpenResult{false, "Failed to execute command: " + config.command};
  }

  return OpenResult{true, ""};
}

FileOpener::OpenResult
FileOpener::openWithDefault(const std::string &file_path) {
  // Validate file path
  auto canonical_path = validatePath(file_path);
  if (!canonical_path) {
    return OpenResult{false, "Invalid or inaccessible file path"};
  }

  std::string path_str = canonical_path->string();

#ifdef _WIN32
  // Windows: use ShellExecute with "open" verb
  SHELLEXECUTEINFOA sei = {sizeof(sei)};
  sei.lpFile = path_str.c_str();
  sei.nShow = SW_SHOWNORMAL;
  sei.lpVerb = "open";
  sei.fMask = SEE_MASK_FLAG_NO_UI;

  if (!ShellExecuteExA(&sei)) {
    return OpenResult{false, "Failed to open with default handler"};
  }

  return OpenResult{true, ""};

#elif defined(__APPLE__)
  // macOS: use 'open' command
  pid_t pid = fork();
  if (pid < 0) {
    return OpenResult{false, "Failed to fork process"};
  }

  if (pid == 0) {
    const char *args[] = {"open", path_str.c_str(), nullptr};
    execvp("open", const_cast<char *const *>(args));
    _exit(127);
  }

  return OpenResult{true, ""};

#else
  // Linux: use xdg-open
  pid_t pid = fork();
  if (pid < 0) {
    return OpenResult{false, "Failed to fork process"};
  }

  if (pid == 0) {
    const char *args[] = {"xdg-open", path_str.c_str(), nullptr};
    execvp("xdg-open", const_cast<char *const *>(args));
    _exit(127);
  }

  return OpenResult{true, ""};
#endif
}

} // namespace fx