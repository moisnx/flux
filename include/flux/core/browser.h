#pragma once

#include <cstddef>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace flux {

class Browser {
public:
  enum class SortMode { NAME, SIZE, DATE, TYPE };

  struct FileEntry {
    std::string name;
    fs::path full_path;
    bool is_directory;
    bool is_symlink;
    bool is_executable;
    bool is_hidden;
    uintmax_t size;
    std::time_t modified_time;

    FileEntry()
        : is_directory(false), is_symlink(false), is_executable(false),
          is_hidden(false), size(0), modified_time(0) {}
  };

  Browser();
  explicit Browser(const fs::path &initial_path);

  // Navigation
  bool loadDirectory(const fs::path &path);
  bool navigateUp();
  bool navigateInto(size_t index);
  bool refresh() { return loadDirectory(current_path_); }

  // Selection movement
  void selectNext();
  void selectPrevious();
  void selectFirst();
  void selectLast();
  void pageDown(size_t page_size);
  void pageUp(size_t page_size);

  // Getters
  const std::vector<FileEntry> &getEntries() const { return entries_; }

  std::optional<FileEntry> getEntryByIndex(size_t index) const {
    if (index >= entries_.size()) {
      return std::nullopt;
    }

    return entries_[index];
  }

  size_t getSelectedIndex() const { return selected_index_; }
  size_t getScrollOffset() const { return scroll_offset_; }
  const fs::path &getCurrentPath() const { return current_path_; }
  const std::string &getFilter() const { return filter_; }
  bool getShowHidden() const { return show_hidden_; }
  SortMode getSortMode() const { return sort_mode_; }
  const std::string &getErrorMessage() const { return error_message_; }
  bool hasError() const { return !error_message_.empty(); }

  std::optional<fs::path> getSelectedPath() const;
  bool isSelectedDirectory() const;

  // Actions
  void toggleHidden() {
    show_hidden_ = !show_hidden_;
    refresh();
  }

  void cycleSortMode();
  void updateScroll(size_t viewport_height);

  // File actions
  bool deleteFile(size_t index);
  bool createFile(const std::string &name);
  bool createDirectory(const std::string &name);
  bool renameEntry(size_t index, const std::string &new_name);
  bool removeEntry(size_t index);

  bool executePaste(const std::vector<fs::path> &source_paths, bool is_cut);

  // Statistics
  size_t getDirectoryCount() const;
  size_t getFileCount() const;
  size_t getTotalEntries() const { return entries_.size(); }

private:
  fs::path current_path_;
  std::vector<FileEntry> entries_;
  size_t selected_index_;
  size_t scroll_offset_;
  std::string filter_;
  std::string error_message_;
  bool show_hidden_;
  SortMode sort_mode_;

  void setError(const std::string &msg) { error_message_ = msg; }
  void clearError() { error_message_.clear(); }
  void sortEntries();
  FileEntry createEntry(const fs::directory_entry &entry);
  bool isExecutable(const fs::path &path) const;
};

} // namespace flux