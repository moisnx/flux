#include "include/file_opener.hpp"
#include <algorithm>
#include <fcntl.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#include <windows.h>
#else
#include <errno.h>
#include <poll.h>
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

bool FileOpener::use_whitelist_ = false;
FileOpener::TerminalCallback FileOpener::suspend_callback_ = nullptr;
FileOpener::TerminalCallback FileOpener::resume_callback_ = nullptr;

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
    if (!std::filesystem::exists(path)) {
      return std::nullopt;
    }

    auto canonical = std::filesystem::canonical(path);

    if (base_dir) {
      auto base_canonical = std::filesystem::canonical(*base_dir);
      auto relative = std::filesystem::relative(canonical, base_canonical);

      std::string rel_str = relative.string();
      if (rel_str.size() >= 2 && rel_str.substr(0, 2) == "..") {
        return std::nullopt;
      }
    }

    return canonical;

  } catch (const std::filesystem::filesystem_error &) {
    return std::nullopt;
  }
}

bool FileOpener::isCommandAllowed(const std::string &command) {
  if (!use_whitelist_) {
    return true;
  }

  auto parts = parseCommand(command);
  if (parts.empty()) {
    return false;
  }

  std::string exe = parts[0];

  size_t slash_pos = exe.find_last_of("/\\");
  if (slash_pos != std::string::npos) {
    exe = exe.substr(slash_pos + 1);
  }

#ifdef _WIN32
  if (exe.size() > 4 && exe.substr(exe.size() - 4) == ".exe") {
    exe = exe.substr(0, exe.size() - 4);
  }
#endif

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
  if (suspend_callback_)
    suspend_callback_();

#ifndef _WIN32
  pid_t parent_pgid = getpgrp();
  int tty_fd = -1;

  if (isatty(STDIN_FILENO)) {
    tty_fd = open("/dev/tty", O_RDWR);
  }
#endif

  pid_t pid = fork();

  if (pid < 0) {
#ifndef _WIN32
    if (tty_fd >= 0)
      close(tty_fd);
#endif
    if (resume_callback_)
      resume_callback_();
    return false;
  }

  if (pid == 0) {
    // Child process

    if (wait) {
#ifndef _WIN32
      if (tty_fd >= 0) {
        // Create new process group
        setpgid(0, 0);

        // Become foreground process
        tcsetpgrp(tty_fd, getpid());

        close(tty_fd);
      }

      // Reset ALL signal handlers to defaults
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      signal(SIGTSTP, SIG_DFL);
      signal(SIGCONT, SIG_DFL);
      signal(SIGWINCH, SIG_DFL);
      signal(SIGTTIN, SIG_DFL);
      signal(SIGTTOU, SIG_DFL);

      // CRITICAL: Unblock all signals for the child
      sigset_t mask;
      sigemptyset(&mask);
      sigprocmask(SIG_SETMASK, &mask, nullptr);
#endif
    } else {
      // GUI apps - fully detach
#ifndef _WIN32
      if (tty_fd >= 0)
        close(tty_fd);
      setsid();

      int devnull = open("/dev/null", O_RDWR);
      if (devnull != -1) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > 2)
          close(devnull);
      }
#endif
    }

    // Build and execute
    std::vector<const char *> args;
    args.push_back(parts[0].c_str());
    for (size_t i = 1; i < parts.size(); ++i)
      args.push_back(parts[i].c_str());

    std::string path_str = file_path.string();
    args.push_back(path_str.c_str());
    args.push_back(nullptr);

    execvp(args[0], const_cast<char *const *>(args.data()));
    _exit(127);
  }

  // Parent process
  if (wait) {
#ifndef _WIN32
    if (tty_fd >= 0) {
      // Set child as its own process group
      setpgid(pid, pid);

      // Make child foreground
      tcsetpgrp(tty_fd, pid);
    }

    // CHANGE: Use SIG_IGN instead of blocking
    // Save old handlers and set signals to ignore (not block!)
    struct sigaction old_winch, old_tstp, old_cont, old_ttin, old_ttou;
    struct sigaction ignore_action;
    ignore_action.sa_handler = SIG_IGN;
    sigemptyset(&ignore_action.sa_mask);
    ignore_action.sa_flags = 0;

    sigaction(SIGWINCH, &ignore_action, &old_winch);
    sigaction(SIGTSTP, &ignore_action, &old_tstp);
    sigaction(SIGCONT, &ignore_action, &old_cont);
    sigaction(SIGTTIN, &ignore_action, &old_ttin);
    sigaction(SIGTTOU, &ignore_action, &old_ttou);
#endif

    int status;
    pid_t result;

    // Wait for child
    do {
      result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);

#ifndef _WIN32
    // Take back terminal control
    if (tty_fd >= 0) {
      // usleep(\1); // 20ms
      tcsetpgrp(tty_fd, parent_pgid);
      close(tty_fd);
    }

    // usleep(\1); // 30ms

    // Restore old signal handlers
    sigaction(SIGWINCH, &old_winch, nullptr);
    sigaction(SIGTSTP, &old_tstp, nullptr);
    sigaction(SIGCONT, &old_cont, nullptr);
    sigaction(SIGTTIN, &old_ttin, nullptr);
    sigaction(SIGTTOU, &old_ttou, nullptr);
#endif

    if (resume_callback_)
      resume_callback_();

    if (result == -1) {
      return false;
    }

    if (WIFEXITED(status)) {
      return WEXITSTATUS(status) == 0;
    }

    return false;
  } else {
    // Non-blocking
#ifndef _WIN32
    if (tty_fd >= 0)
      close(tty_fd);
#endif
    // usleep(\1);
    if (resume_callback_)
      resume_callback_();
  }

  return true;
}
#endif

#ifdef _WIN32
bool FileOpener::executeWindows(const std::string &command,
                                const std::filesystem::path &file_path,
                                bool wait) {
  if (suspend_callback_)
    suspend_callback_();

  auto parts = parseCommand(command);
  if (parts.empty()) {
    if (resume_callback_)
      resume_callback_();
    return false;
  }

  std::string exe = parts[0];
  std::string args;

  for (size_t i = 1; i < parts.size(); ++i) {
    if (!args.empty())
      args += " ";
    args += "\"" + parts[i] + "\"";
  }

  if (!args.empty())
    args += " ";
  args += "\"" + file_path.string() + "\"";

  SHELLEXECUTEINFOA sei = {sizeof(sei)};
  sei.lpFile = exe.c_str();
  sei.lpParameters = args.empty() ? nullptr : args.c_str();
  sei.nShow = SW_SHOWNORMAL;
  sei.fMask = wait ? SEE_MASK_NOCLOSEPROCESS : 0;

  if (!ShellExecuteExA(&sei)) {
    if (resume_callback_)
      resume_callback_();
    return false;
  }

  if (wait && sei.hProcess) {
    WaitForSingleObject(sei.hProcess, INFINITE);
    CloseHandle(sei.hProcess);
  }

  if (resume_callback_)
    resume_callback_();
  return true;
}
#endif

FileOpener::OpenResult FileOpener::openWith(const std::string &file_path,
                                            const OpenConfig &config) {
  auto canonical_path = validatePath(file_path, config.allowed_base_dir);
  if (!canonical_path) {
    return OpenResult{false, "Invalid or inaccessible file path"};
  }

  if (config.validate_command && !isCommandAllowed(config.command)) {
    return OpenResult{false,
                      "Command not in allowed whitelist: " + config.command};
  }

  auto parts = parseCommand(config.command);
  if (parts.empty()) {
    return OpenResult{false, "Empty command"};
  }

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
  auto canonical_path = validatePath(file_path);
  if (!canonical_path) {
    return OpenResult{false, "Invalid or inaccessible file path"};
  }

  std::string path_str = canonical_path->string();

  if (suspend_callback_)
    suspend_callback_();

#ifdef _WIN32
  SHELLEXECUTEINFOA sei = {sizeof(sei)};
  sei.lpFile = path_str.c_str();
  sei.nShow = SW_SHOWNORMAL;
  sei.lpVerb = "open";
  sei.fMask = SEE_MASK_FLAG_NO_UI;

  bool success = ShellExecuteExA(&sei);

  if (resume_callback_)
    resume_callback_();

  if (!success) {
    return OpenResult{false, "Failed to open with default handler"};
  }

  return OpenResult{true, ""};

#elif defined(__APPLE__)
  pid_t pid = fork();
  if (pid < 0) {
    if (resume_callback_)
      resume_callback_();
    return OpenResult{false, "Failed to fork process"};
  }

  if (pid == 0) {
    // Redirect output for GUI apps
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    setsid();

    const char *args[] = {"open", path_str.c_str(), nullptr};
    execvp("open", const_cast<char *const *>(args));
    _exit(127);
  }

  if (resume_callback_)
    resume_callback_();
  return OpenResult{true, ""};

#else
  pid_t pid = fork();
  if (pid < 0) {
    if (resume_callback_)
      resume_callback_();
    return OpenResult{false, "Failed to fork process"};
  }

  if (pid == 0) {
    // Redirect output to prevent terminal corruption
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    setsid();

    const char *args[] = {"xdg-open", path_str.c_str(), nullptr};
    execvp("xdg-open", const_cast<char *const *>(args));
    _exit(127);
  }

  // Small delay to let xdg-open spawn its child
  // usleep(\1); // 100ms

  if (resume_callback_)
    resume_callback_();
  return OpenResult{true, ""};
#endif
}

} // namespace fx