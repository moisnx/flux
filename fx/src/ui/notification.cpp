#include "include/ui/notification.hpp"
#include <algorithm>
#include <sstream>

namespace fx {

// ============================================================================
// NotificationManager Implementation
// ============================================================================

NotificationManager::NotificationManager(notcurses *nc, const Theme &theme)
    : nc_(nc), notif_plane_(nullptr), theme_(theme) {
  recreatePlane();
}

NotificationManager::~NotificationManager() {
  if (notif_plane_) {
    ncplane_destroy(notif_plane_);
  }
  clear();
}

void NotificationManager::recreatePlane() {
  // Destroy old plane
  if (notif_plane_) {
    ncplane_destroy(notif_plane_);
    notif_plane_ = nullptr;
  }

  if (!nc_) {
    return; // Early return if no notcurses context
  }

  ncplane *stdplane = notcurses_stdplane(nc_);
  if (!stdplane) {
    return; // Early return if no stdplane
  }

  unsigned width, height;
  ncplane_dim_yx(stdplane, &height, &width);

  // Safety check for dimensions
  if (width == 0 || height == 0) {
    return;
  }

  int notif_y = (int)height - (int)MAX_NOTIFICATIONS - 2;
  if (notif_y < 0)
    notif_y = 0;

  struct ncplane_options opts = {.y = notif_y,
                                 .x = 0,
                                 .rows = MAX_NOTIFICATIONS,
                                 .cols = width,
                                 .userptr = nullptr,
                                 .name = "notifications",
                                 .resizecb = nullptr,
                                 .flags = 0,
                                 .margin_b = 0,
                                 .margin_r = 0};

  notif_plane_ = ncplane_create(stdplane, &opts);
  if (!notif_plane_) {
    return;
  }

  uint64_t channels = 0;
  ncchannels_set_fg_rgb8(&channels, (theme_.foreground >> 16) & 0xFF,
                         (theme_.foreground >> 8) & 0xFF,
                         theme_.foreground & 0xFF);

  if (theme_.use_default_bg) {
    ncchannels_set_bg_default(&channels);
  } else {
    ncchannels_set_bg_rgb8(&channels, (theme_.background >> 16) & 0xFF,
                           (theme_.background >> 8) & 0xFF,
                           theme_.background & 0xFF);
  }

  ncplane_set_base(notif_plane_, " ", 0, channels);
}

void NotificationManager::cleanup() {
  // Destroy plane before notcurses is stopped
  if (notif_plane_) {
    ncplane_destroy(notif_plane_);
    notif_plane_ = nullptr;
  }
  nc_ = nullptr;
}

void NotificationManager::handleResize() { recreatePlane(); }

void NotificationManager::updateNotcursesPointer(notcurses *nc) {
  nc_ = nc;
  recreatePlane(); // Recreate plane with new notcurses instance
}

void NotificationManager::info(const std::string &message, int duration_ms) {
  show(message, NotificationType::INFO, duration_ms);
}

void NotificationManager::success(const std::string &message, int duration_ms) {
  show(message, NotificationType::SUCCESS, duration_ms);
}

void NotificationManager::warning(const std::string &message, int duration_ms) {
  show(message, NotificationType::WARNING, duration_ms);
}

void NotificationManager::error(const std::string &message, int duration_ms) {
  show(message, NotificationType::ERROR, duration_ms);
}

void NotificationManager::hint(const std::string &message, int duration_ms) {
  show(message, NotificationType::HINT, duration_ms);
}

void NotificationManager::show(const std::string &message,
                               NotificationType type, int duration_ms,
                               bool dismissable) {
  pruneExpired();

  // Limit max notifications
  if (notifications_.size() >= MAX_NOTIFICATIONS) {
    notifications_.erase(notifications_.begin());
  }

  Notification notif;
  notif.message = message;
  notif.type = type;
  notif.created = std::chrono::steady_clock::now();
  notif.duration_ms = duration_ms;
  notif.dismissable = dismissable;

  notifications_.push_back(notif);
}

void NotificationManager::render(ncplane *plane,
                                 NotificationPosition position) {
  if (!notif_plane_) {
    recreatePlane();
    if (!notif_plane_)
      return;
  }

  // Clear our dedicated plane
  ncplane_erase(notif_plane_);

  if (notifications_.empty()) {
    return;
  }

  unsigned width, height;
  ncplane_dim_yx(notif_plane_, &height, &width);

  // Safety check for dimensions
  if (width == 0 || height == 0)
    return;

  int y = 0; // Start from top of our plane
  for (const auto &notif : notifications_) {
    if (y >= (int)height)
      break; // Don't overflow our plane

    uint32_t fg_color = getColorForType(notif.type);
    uint32_t bg_color = theme_.status_bar;
    std::string icon = getIconForType(notif.type);

    std::string display = " " + icon + " " + notif.message + " ";
    int max_width = width - 8;
    if (max_width < 10)
      max_width = 10; // Minimum width

    if (display.length() > (size_t)max_width) {
      display = display.substr(0, max_width - 3) + "... ";
    }

    int display_width = display.length() + 4;
    int x = (width - display_width) / 2;
    if (x < 0)
      x = 0; // Safety check

    // Background channel
    uint64_t bg_channels = 0;
    ncchannels_set_fg_rgb8(&bg_channels, (theme_.foreground >> 16) & 0xFF,
                           (theme_.foreground >> 8) & 0xFF,
                           theme_.foreground & 0xFF);
    ncchannels_set_bg_rgb8(&bg_channels, (bg_color >> 16) & 0xFF,
                           (bg_color >> 8) & 0xFF, bg_color & 0xFF);

    ncplane_set_channels(notif_plane_, bg_channels);
    ncplane_putstr_yx(notif_plane_, y, x, "│");

    // Message colors
    uint64_t msg_channels = 0;
    ncchannels_set_fg_rgb8(&msg_channels, (fg_color >> 16) & 0xFF,
                           (fg_color >> 8) & 0xFF, fg_color & 0xFF);
    ncchannels_set_bg_rgb8(&msg_channels, (bg_color >> 16) & 0xFF,
                           (bg_color >> 8) & 0xFF, bg_color & 0xFF);

    ncplane_set_channels(notif_plane_, msg_channels);
    ncplane_putstr_yx(notif_plane_, y, x + 1, display.c_str());

    // Right border
    ncplane_set_channels(notif_plane_, bg_channels);
    ncplane_putstr_yx(notif_plane_, y, x + display.length() + 1, "│");

    y++;
  }
}

void NotificationManager::clear() { notifications_.clear(); }

void NotificationManager::dismissLast() {
  if (!notifications_.empty()) {
    notifications_.pop_back();
  }
}

void NotificationManager::setTheme(const Theme &theme) {
  theme_ = theme;
  recreatePlane();
}

bool NotificationManager::hasNotifications() const {
  return !notifications_.empty();
}

size_t NotificationManager::getNotificationCount() const {
  return notifications_.size();
}

std::optional<std::chrono::steady_clock::time_point>
NotificationManager::getNextExpiryTime() const {
  if (notifications_.empty()) {
    return std::nullopt;
  }

  auto now = std::chrono::steady_clock::now();
  std::optional<std::chrono::steady_clock::time_point> next_expiry;

  for (const auto &notif : notifications_) {
    if (notif.duration_ms == 0)
      continue; // Skip persistent

    auto expiry = notif.created + std::chrono::milliseconds(notif.duration_ms);

    // ONLY consider expiry times in the FUTURE
    if (expiry <= now)
      continue; // Skip already expired notifications

    if (!next_expiry || expiry < *next_expiry) {
      next_expiry = expiry;
    }
  }

  return next_expiry;
}

void NotificationManager::pruneExpired() {
  auto now = std::chrono::steady_clock::now();
  notifications_.erase(
      std::remove_if(
          notifications_.begin(), notifications_.end(),
          [now](const Notification &n) {
            if (n.duration_ms == 0)
              return false; // Persistent
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                      n.created)
                    .count();
            return elapsed >= n.duration_ms;
          }),
      notifications_.end());
}

uint32_t NotificationManager::getColorForType(NotificationType type) const {
  switch (type) {
  case NotificationType::INFO:
    return theme_.ui_info;
  case NotificationType::SUCCESS:
    return theme_.ui_success;
  case NotificationType::WARNING:
    return theme_.ui_warning;
  case NotificationType::ERROR:
    return theme_.ui_error;
  case NotificationType::HINT:
    return theme_.ui_accent;
  default:
    return theme_.foreground;
  }
}

std::string NotificationManager::getIconForType(NotificationType type) const {
  switch (type) {
  case NotificationType::INFO:
    return "ℹ";
  case NotificationType::SUCCESS:
    return "✓";
  case NotificationType::WARNING:
    return "⚠";
  case NotificationType::ERROR:
    return "✗";
  case NotificationType::HINT:
    return "💡";
  default:
    return "•";
  }
}

// ============================================================================
// MessageBox Implementation
// ============================================================================

MessageBox::Result MessageBox::show(notcurses *nc, ncplane *stdplane,
                                    const Config &config, const Theme &theme) {
  unsigned screen_width, screen_height;
  ncplane_dim_yx(stdplane, &screen_height, &screen_width);

  // Calculate box width first (adaptive to screen size)
  unsigned box_width =
      std::min(std::max(50u, screen_width * 80 / 100), screen_width - 4);

  // Text wrapping width should be box_width minus padding/borders
  unsigned text_width = box_width - 8; // Leave room for borders and padding

  // Wrap message text with adaptive width
  auto lines = wrapText(config.message, text_width);

  // Calculate box height - ensure it fits on screen
  unsigned content_height = lines.size() + 8;  // Header + buttons + padding
  unsigned max_box_height = screen_height - 4; // Leave some margin
  unsigned box_height = std::min(content_height, max_box_height);
  box_height = std::max(box_height, 10u); // Minimum height

  // If content is too tall, we may need scrolling (for now just truncate)
  unsigned max_visible_lines = box_height - 8;
  if (lines.size() > max_visible_lines) {
    lines.resize(max_visible_lines);
    if (max_visible_lines > 0) {
      lines[max_visible_lines - 1] += "...";
    }
  }

  int box_y = (screen_height - box_height) / 2;
  int box_x = (screen_width - box_width) / 2;

  // Create modal plane
  struct ncplane_options opts = {.y = box_y,
                                 .x = box_x,
                                 .rows = box_height,
                                 .cols = box_width,
                                 .userptr = nullptr,
                                 .name = "messagebox",
                                 .resizecb = nullptr,
                                 .flags = 0,
                                 .margin_b = 0,
                                 .margin_r = 0};

  ncplane *modal = ncplane_create(stdplane, &opts);
  if (!modal)
    return Result::CANCEL;

  // Set background
  uint64_t bg_channels = 0;
  ncchannels_set_fg_rgb8(&bg_channels, (theme.foreground >> 16) & 0xFF,
                         (theme.foreground >> 8) & 0xFF,
                         theme.foreground & 0xFF);
  ncchannels_set_bg_rgb8(&bg_channels, (theme.background >> 16) & 0xFF,
                         (theme.background >> 8) & 0xFF,
                         theme.background & 0xFF);

  ncplane_set_channels(modal, bg_channels);

  // Fill background first
  ncplane_erase(modal);
  for (unsigned y = 0; y < box_height; ++y) {
    for (unsigned x = 0; x < box_width; ++x) {
      ncplane_putchar_yx(modal, y, x, ' ');
    }
  }

  // Draw border with ui_border color
  uint64_t border_channels = 0;
  ncchannels_set_fg_rgb8(&border_channels, (theme.ui_border >> 16) & 0xFF,
                         (theme.ui_border >> 8) & 0xFF, theme.ui_border & 0xFF);
  ncchannels_set_bg_rgb8(&border_channels, (theme.background >> 16) & 0xFF,
                         (theme.background >> 8) & 0xFF,
                         theme.background & 0xFF);

  ncplane_set_channels(modal, border_channels);

  // Draw box borders
  for (unsigned x = 1; x < box_width - 1; ++x) {
    ncplane_putstr_yx(modal, 0, x, "─");
    ncplane_putstr_yx(modal, box_height - 1, x, "─");
  }
  for (unsigned y = 1; y < box_height - 1; ++y) {
    ncplane_putstr_yx(modal, y, 0, "│");
    ncplane_putstr_yx(modal, y, box_width - 1, "│");
  }
  ncplane_putstr_yx(modal, 0, 0, "┌");
  ncplane_putstr_yx(modal, 0, box_width - 1, "┐");
  ncplane_putstr_yx(modal, box_height - 1, 0, "└");
  ncplane_putstr_yx(modal, box_height - 1, box_width - 1, "┘");

  // Title with appropriate color
  uint32_t title_color = getColorForType(config.type, theme);
  uint64_t title_channels = 0;
  ncchannels_set_fg_rgb8(&title_channels, (title_color >> 16) & 0xFF,
                         (title_color >> 8) & 0xFF, title_color & 0xFF);
  ncchannels_set_bg_rgb8(&title_channels, (theme.background >> 16) & 0xFF,
                         (theme.background >> 8) & 0xFF,
                         theme.background & 0xFF);

  ncplane_set_channels(modal, title_channels);

  std::string icon = getIconForType(config.type);
  std::string title_str = " " + icon + " " + config.title + " ";
  int title_x = (box_width - title_str.length()) / 2;
  ncplane_putstr_yx(modal, 1, title_x, title_str.c_str());

  // Separator line
  ncplane_set_channels(modal, border_channels);
  for (unsigned x = 1; x < box_width - 1; ++x) {
    ncplane_putstr_yx(modal, 2, x, "─");
  }

  // Message lines (normal foreground) - left-align for better readability
  ncplane_set_channels(modal, bg_channels);
  int y = 4;
  for (const auto &line : lines) {
    if (!line.empty()) {
      // Left-align text with padding instead of centering
      int x = 3; // Left padding from border
      ncplane_putstr_yx(modal, y, x, line.c_str());
    }
    y++;
  }

  // Buttons
  std::vector<std::string> buttons;
  if (config.type == Type::CONFIRM) {
    buttons = {"[Y]es", "[N]o"};
    if (config.show_cancel)
      buttons.push_back("[C]ancel");
  } else {
    buttons = {"[OK]"};
  }

  // Calculate button positions
  int total_width = 0;
  for (const auto &btn : buttons)
    total_width += btn.length() + 4;

  int btn_y = box_height - 2;
  int btn_x = (box_width - total_width) / 2;

  // Draw buttons with accent color
  uint64_t btn_channels = 0;
  ncchannels_set_fg_rgb8(&btn_channels, (theme.ui_accent >> 16) & 0xFF,
                         (theme.ui_accent >> 8) & 0xFF, theme.ui_accent & 0xFF);
  ncchannels_set_bg_rgb8(&btn_channels, (theme.background >> 16) & 0xFF,
                         (theme.background >> 8) & 0xFF,
                         theme.background & 0xFF);

  ncplane_set_channels(modal, btn_channels);
  for (const auto &btn : buttons) {
    ncplane_putstr_yx(modal, btn_y, btn_x, btn.c_str());
    btn_x += btn.length() + 4;
  }

  // Force render the modal
  notcurses_render(nc);

  // Flush any pending input before waiting for user response
  ncinput flush_input;
  while (notcurses_get_nblock(nc, &flush_input) != 0) {
    // Discard any queued input
  }

  // Wait for input
  Result result = Result::CANCEL;
  ncinput ni;
  while (true) {
    uint32_t key = notcurses_get_blocking(nc, &ni);

    if (config.type == Type::CONFIRM) {
      if (key == 'y' || key == 'Y') {
        result = Result::YES;
        break;
      } else if (key == 'n' || key == 'N') {
        result = Result::NO;
        break;
      } else if (config.show_cancel &&
                 (key == 'c' || key == 'C' || key == 27)) {
        result = Result::CANCEL;
        break;
      } else if (key == 27 && !config.show_cancel) {
        // ESC behaves like NO if cancel not shown
        result = Result::NO;
        break;
      }
    } else {
      if (key == 10 || key == 13 || key == ' ') {
        result = Result::OK;
        break;
      } else if (key == 27) {
        result = Result::CANCEL;
        break;
      }
    }
  }

  ncplane_destroy(modal);
  notcurses_render(nc); // Re-render after destroying modal

  // Flush any additional pending input after dialog closes
  while (notcurses_get_nblock(nc, &flush_input) != 0) {
    // Discard any queued input
  }

  return result;
}

void MessageBox::info(notcurses *nc, ncplane *stdplane,
                      const std::string &title, const std::string &message,
                      const Theme &theme) {
  Config config;
  config.title = title;
  config.message = message;
  config.type = Type::INFO;
  show(nc, stdplane, config, theme);
}

void MessageBox::error(notcurses *nc, ncplane *stdplane,
                       const std::string &title, const std::string &message,
                       const Theme &theme) {
  Config config;
  config.title = title;
  config.message = message;
  config.type = Type::ERROR;
  show(nc, stdplane, config, theme);
}

bool MessageBox::confirm(notcurses *nc, ncplane *stdplane,
                         const std::string &title, const std::string &message,
                         const Theme &theme) {
  Config config;
  config.title = title;
  config.message = message;
  config.type = Type::CONFIRM;
  config.show_cancel = true;
  return show(nc, stdplane, config, theme) == Result::YES;
}

std::vector<std::string> MessageBox::wrapText(const std::string &text,
                                              unsigned max_width) {
  std::vector<std::string> lines;

  // Handle empty text
  if (text.empty()) {
    lines.push_back("");
    return lines;
  }

  // Ensure minimum width
  if (max_width < 10)
    max_width = 10;

  // Split by newlines first (preserve intentional line breaks)
  std::istringstream paragraph_stream(text);
  std::string paragraph;

  while (std::getline(paragraph_stream, paragraph)) {
    if (paragraph.empty()) {
      lines.push_back("");
      continue;
    }

    // Wrap each paragraph
    std::istringstream word_stream(paragraph);
    std::string word, line;

    while (word_stream >> word) {
      // If adding this word would exceed max_width
      if (!line.empty() && line.length() + word.length() + 1 > max_width) {
        lines.push_back(line);
        line.clear();
      }

      // If the word itself is longer than max_width, split it
      while (word.length() > max_width) {
        if (!line.empty()) {
          lines.push_back(line);
          line.clear();
        }
        // Split long word with hyphen
        lines.push_back(word.substr(0, max_width - 1) + "-");
        word = word.substr(max_width - 1);
      }

      // Add word to current line
      if (!line.empty())
        line += " ";
      line += word;
    }

    // Add remaining line
    if (!line.empty()) {
      lines.push_back(line);
    }
  }

  // Ensure at least one line
  if (lines.empty()) {
    lines.push_back("");
  }

  return lines;
}

uint32_t MessageBox::getColorForType(Type type, const Theme &theme) {
  switch (type) {
  case Type::INFO:
    return theme.ui_info;
  case Type::SUCCESS:
    return theme.ui_success;
  case Type::WARNING:
    return theme.ui_warning;
  case Type::ERROR:
    return theme.ui_error;
  case Type::CONFIRM:
    return theme.ui_accent;
  default:
    return theme.foreground;
  }
}

std::string MessageBox::getIconForType(Type type) {
  switch (type) {
  case Type::INFO:
    return "ℹ";
  case Type::SUCCESS:
    return "✓";
  case Type::WARNING:
    return "⚠";
  case Type::ERROR:
    return "✗";
  case Type::CONFIRM:
    return "?";
  default:
    return "•";
  }
}

// ============================================================================
// StatusBar Implementation
// ============================================================================

StatusBar::StatusBar(const Theme &theme) : theme_(theme) {}

void StatusBar::setLeft(const std::string &text) { left_ = text; }
void StatusBar::setCenter(const std::string &text) { center_ = text; }
void StatusBar::setRight(const std::string &text) { right_ = text; }
void StatusBar::setTheme(const Theme &theme) { theme_ = theme; }

void StatusBar::render(ncplane *plane, unsigned width) {
  unsigned height;
  ncplane_dim_yx(plane, &height, &width);

  int y = height - 1;

  // Set colors using theme status bar colors
  uint64_t channels = 0;
  ncchannels_set_fg_rgb8(&channels, (theme_.foreground >> 16) & 0xFF,
                         (theme_.foreground >> 8) & 0xFF,
                         theme_.foreground & 0xFF);
  ncchannels_set_bg_rgb8(&channels, (theme_.status_bar >> 16) & 0xFF,
                         (theme_.status_bar >> 8) & 0xFF,
                         theme_.status_bar & 0xFF);

  ncplane_set_channels(plane, channels);

  // Clear line with status bar background
  for (unsigned x = 0; x < width; ++x) {
    ncplane_putchar_yx(plane, y, x, ' ');
  }

  // Render sections
  if (!left_.empty()) {
    ncplane_putstr_yx(plane, y, 1, left_.c_str());
  }

  if (!center_.empty()) {
    int x = (width - center_.length()) / 2;
    ncplane_putstr_yx(plane, y, x, center_.c_str());
  }

  if (!right_.empty()) {
    int x = width - right_.length() - 1;
    ncplane_putstr_yx(plane, y, x, right_.c_str());
  }
}

} // namespace fx