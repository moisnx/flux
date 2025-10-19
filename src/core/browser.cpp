#include "flux/core/browser.h"
#include <algorithm>
#include <iostream>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>
#undef min
#undef max
#else
#include <unistd.h>
#endif

using namespace flux;

Browser::Browser()
    : current_path_(fs::current_path()), selected_index_(0), scroll_offset_(0),
      show_hidden_(false), sort_mode_(SortMode::TYPE) {
  loadDirectory(current_path_);
}

Browser::Browser(const fs::path &initial_path)
    : current_path_(initial_path), selected_index_(0), scroll_offset_(0),
      show_hidden_(false), sort_mode_(SortMode::TYPE) {
  if (!loadDirectory(initial_path)) {
    current_path_ = fs::current_path();
    loadDirectory(current_path_);
  }
}

bool Browser::loadDirectory(const fs::path &path) {
  clearError();

  try {
    if (!fs::exists(path)) {
      setError("Path does not exist");
      return false;
    }

    if (!fs::is_directory(path)) {
      setError("Not a directory");
      return false;
    }

    // Use a temporary vector to build the new entries
    std::vector<FileEntry> new_entries;
    fs::path new_path;

    try {
      new_path = fs::canonical(path);
    } catch (const std::exception &e) {
      setError(std::string("Cannot resolve path: ") + e.what());
      return false;
    }

    // Add parent directory entry if not at root
    if (new_path.has_parent_path() && new_path != new_path.root_path()) {
      FileEntry parent;
      parent.name = "..";
      parent.full_path = new_path.parent_path();
      parent.is_directory = true;
      parent.is_symlink = false;
      parent.is_executable = false;
      parent.is_hidden = false;
      parent.size = 0;
      parent.modified_time = 0;
      new_entries.push_back(parent);
    }

    // Read directory contents
    std::error_code ec;
    fs::directory_iterator dir_iter(new_path, ec);

    if (ec) {
      setError(std::string("Cannot read directory: ") + ec.message());
      return false;
    }

    for (const auto &entry : dir_iter) {
      try {
        FileEntry file_entry = createEntry(entry);

        // Apply hidden file filter
        if (!show_hidden_ && file_entry.is_hidden && file_entry.name != "..") {
          continue;
        }

        new_entries.push_back(file_entry);
      } catch (const std::exception &e) {
        // Log but continue processing other entries
        std::cerr << "Warning: Skipping entry due to error: " << e.what()
                  << std::endl;
        continue;
      }
    }

    // Only update state if we successfully read the directory
    entries_ = std::move(new_entries);
    current_path_ = new_path;

    sortEntries();
    selected_index_ = 0;
    scroll_offset_ = 0;
    return true;
  } catch (const fs::filesystem_error &e) {
    setError(std::string("Cannot read: ") + e.what());
    return false;
  } catch (const std::exception &e) {
    setError(std::string("Error: ") + e.what());
    return false;
  }
}

bool Browser::navigateUp() {
  if (current_path_ == current_path_.root_path()) {
    return false;
  }
  return loadDirectory(current_path_.parent_path());
}

bool Browser::navigateInto(size_t index) {
  if (index >= entries_.size()) {
    return false;
  }

  const FileEntry &entry = entries_[index];

  if (!entry.is_directory) {
    return false;
  }

  // Copy the path before calling loadDirectory to avoid reference invalidation
  fs::path target_path = entry.full_path;
  return loadDirectory(target_path);
}

void Browser::selectNext() {
  if (!entries_.empty() && selected_index_ < entries_.size() - 1) {
    selected_index_++;
  }
}

void Browser::selectPrevious() {
  if (!entries_.empty() && selected_index_ > 0) {
    selected_index_--;
  }
}

void Browser::selectFirst() {
  if (!entries_.empty()) {
    selected_index_ = 0;
  }
}

void Browser::selectLast() {
  if (!entries_.empty()) {
    selected_index_ = entries_.size() - 1;
  }
}

void Browser::pageDown(size_t page_size) {
  if (!entries_.empty()) {
    selected_index_ =
        std::min(selected_index_ + page_size, entries_.size() - 1);
  }
}

void Browser::pageUp(size_t page_size) {
  if (!entries_.empty()) {
    if (selected_index_ >= page_size) {
      selected_index_ -= page_size;
    } else {
      selected_index_ = 0;
    }
  }
}

std::optional<fs::path> Browser::getSelectedPath() const {
  if (selected_index_ >= entries_.size()) {
    return std::nullopt;
  }
  return entries_[selected_index_].full_path;
}

bool Browser::isSelectedDirectory() const {
  if (selected_index_ >= entries_.size()) {
    return false;
  }
  return entries_[selected_index_].is_directory;
}

void Browser::cycleSortMode() {
  switch (sort_mode_) {
  case SortMode::TYPE:
    sort_mode_ = SortMode::NAME;
    break;
  case SortMode::NAME:
    sort_mode_ = SortMode::SIZE;
    break;
  case SortMode::SIZE:
    sort_mode_ = SortMode::DATE;
    break;
  case SortMode::DATE:
    sort_mode_ = SortMode::TYPE;
    break;
  }
  sortEntries();
}

void Browser::updateScroll(size_t viewport_height) {
  if (entries_.empty() || viewport_height == 0) {
    scroll_offset_ = 0;
    return;
  }

  // Keep selection in view
  if (selected_index_ < scroll_offset_) {
    scroll_offset_ = selected_index_;
  } else if (selected_index_ >= scroll_offset_ + viewport_height) {
    scroll_offset_ = selected_index_ - viewport_height + 1;
  }
}

size_t Browser::getDirectoryCount() const {
  return std::count_if(
      entries_.begin(), entries_.end(),
      [](const FileEntry &e) { return e.is_directory && e.name != ".."; });
}

size_t Browser::getFileCount() const {
  return std::count_if(entries_.begin(), entries_.end(),
                       [](const FileEntry &e) { return !e.is_directory; });
}

void Browser::sortEntries() {
  if (entries_.empty()) {
    return;
  }

  // Keep ".." at the top
  auto parent_it =
      std::find_if(entries_.begin(), entries_.end(),
                   [](const FileEntry &e) { return e.name == ".."; });

  auto sort_start =
      (parent_it != entries_.end()) ? parent_it + 1 : entries_.begin();

  switch (sort_mode_) {
  case SortMode::NAME:
    std::sort(
        sort_start, entries_.end(),
        [](const FileEntry &a, const FileEntry &b) { return a.name < b.name; });
    break;

  case SortMode::SIZE:
    std::sort(sort_start, entries_.end(),
              [](const FileEntry &a, const FileEntry &b) {
                // Directories first, then by size
                if (a.is_directory != b.is_directory) {
                  return a.is_directory > b.is_directory;
                }
                return a.size > b.size;
              });
    break;

  case SortMode::DATE:
    std::sort(sort_start, entries_.end(),
              [](const FileEntry &a, const FileEntry &b) {
                return a.modified_time > b.modified_time;
              });
    break;

  case SortMode::TYPE:
    // Directories first, then files alphabetically
    std::sort(sort_start, entries_.end(),
              [](const FileEntry &a, const FileEntry &b) {
                if (a.is_directory != b.is_directory) {
                  return a.is_directory > b.is_directory;
                }
                return a.name < b.name;
              });
    break;
  }
}

Browser::FileEntry Browser::createEntry(const fs::directory_entry &entry) {
  FileEntry file_entry;

  try {
    file_entry.name = entry.path().filename().string();
    file_entry.full_path = entry.path();

    std::error_code ec;
    file_entry.is_directory = entry.is_directory(ec);
    if (ec) {
      file_entry.is_directory = false;
    }

    file_entry.is_symlink = entry.is_symlink(ec);
    if (ec) {
      file_entry.is_symlink = false;
    }

    file_entry.is_hidden =
        !file_entry.name.empty() && file_entry.name[0] == '.';

    if (!file_entry.is_directory) {
      try {
        file_entry.size = entry.file_size(ec);
        if (ec) {
          file_entry.size = 0;
        }
        file_entry.is_executable = isExecutable(entry.path());
      } catch (...) {
        file_entry.size = 0;
        file_entry.is_executable = false;
      }
    } else {
      file_entry.size = 0;
      file_entry.is_executable = false;
    }

    // Get modification time with better error handling
    try {
      auto ftime = entry.last_write_time(ec);
      if (!ec) {
        // Simpler, more portable time conversion
        auto sctp =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() +
                std::chrono::system_clock::now());
        file_entry.modified_time = std::chrono::system_clock::to_time_t(sctp);
      } else {
        file_entry.modified_time = 0;
      }
    } catch (...) {
      file_entry.modified_time = 0;
    }
  } catch (const std::exception &e) {
    std::cerr << "Warning: Error reading entry " << entry.path() << ": "
              << e.what() << std::endl;
    // Set safe defaults
    file_entry.is_directory = false;
    file_entry.is_symlink = false;
    file_entry.is_executable = false;
    file_entry.is_hidden = false;
    file_entry.size = 0;
    file_entry.modified_time = 0;
  }

  return file_entry;
}

bool Browser::isExecutable(const fs::path &path) const {
#ifdef _WIN32
  // On Windows, check file extension
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com";
#else
  // On Unix/Linux/macOS, check execute permission bits
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    return (st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) ||
           (st.st_mode & S_IXOTH);
  }
  return false;
#endif
}
