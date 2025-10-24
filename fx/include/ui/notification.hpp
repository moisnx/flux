#pragma once

#include "theme.hpp"
#include <chrono>
#include <notcurses/notcurses.h>
#include <string>
#include <vector>

namespace fx {

enum class NotificationType { INFO, SUCCESS, WARNING, ERROR, HINT };

enum class NotificationPosition { TOP, BOTTOM, CENTER };

struct Notification {
  std::string message;
  NotificationType type;
  std::chrono::steady_clock::time_point created;
  int duration_ms; // 0 = permanent until dismissed
  bool dismissable;
};

/**
 * Manages non-blocking notifications (toasts/banners)
 * Displayed at top or bottom of screen
 */
class NotificationManager {
public:
  NotificationManager(notcurses *nc, const Theme &theme);
  ~NotificationManager();

  // Add notifications
  void info(const std::string &message, int duration_ms = 3000);
  void success(const std::string &message, int duration_ms = 3000);
  void warning(const std::string &message, int duration_ms = 5000);
  void error(const std::string &message, int duration_ms = 0); // 0 = persistent
  void hint(const std::string &message, int duration_ms = 2000);

  // Custom notification
  void show(const std::string &message, NotificationType type,
            int duration_ms = 3000, bool dismissable = true);

  // Management
  void render(ncplane *plane,
              NotificationPosition position = NotificationPosition::BOTTOM);
  void clear();
  void cleanup();
  void dismissLast();
  void setTheme(const Theme &theme);

  // Check if any notifications are visible
  bool hasNotifications() const;
  size_t getNotificationCount() const;

  std::optional<std::chrono::steady_clock::time_point>
  getNextExpiryTime() const;

  void pruneExpired();
  void handleResize();
  void updateNotcursesPointer(notcurses *nc);

private:
  uint32_t getColorForType(NotificationType type) const;
  std::string getIconForType(NotificationType type) const;

  notcurses *nc_;
  Theme theme_;
  ncplane *notif_plane_;
  std::vector<Notification> notifications_;
  static constexpr size_t MAX_NOTIFICATIONS = 3;
  void recreatePlane();
};

/**
 * Modal message boxes for important messages requiring user attention
 */
class MessageBox {
public:
  enum class Type { INFO, SUCCESS, WARNING, ERROR, CONFIRM };

  enum class Result { OK, YES, NO, CANCEL };

  struct Config {
    std::string title;
    std::string message;
    Type type = Type::INFO;
    bool show_cancel = false;
    std::string ok_text = "OK";
    std::string cancel_text = "Cancel";
  };

  static Result show(notcurses *nc, ncplane *stdplane, const Config &config,
                     const Theme &theme);

  // Convenience methods
  static void info(notcurses *nc, ncplane *stdplane, const std::string &title,
                   const std::string &message, const Theme &theme);

  static void error(notcurses *nc, ncplane *stdplane, const std::string &title,
                    const std::string &message, const Theme &theme);

  static bool confirm(notcurses *nc, ncplane *stdplane,
                      const std::string &title, const std::string &message,
                      const Theme &theme);

private:
  static std::vector<std::string> wrapText(const std::string &text,
                                           unsigned max_width);
  static uint32_t getColorForType(Type type, const Theme &theme);
  static std::string getIconForType(Type type);
};

/**
 * Status bar for persistent contextual information
 */
class StatusBar {
public:
  StatusBar(const Theme &theme);

  void setLeft(const std::string &text);
  void setCenter(const std::string &text);
  void setRight(const std::string &text);
  void setTheme(const Theme &theme);

  void render(ncplane *plane, unsigned width);

private:
  Theme theme_;
  std::string left_;
  std::string center_;
  std::string right_;
};

} // namespace fx