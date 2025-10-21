#include "flux/core/file_clipboard.h"
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>

#elif __APPLE__
#include <AppleKit/AppleKit.h>
#include <CoreFoundation/CoreFoundation.h>

#elif __linux__
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#endif

namespace flux {
FileClipboard::Operation FileClipboard::currentOperation = Operation::Copy;
#ifdef _WIN32
// Windows implementation
bool FileClipboard::copyFiles(const std::vector<std::string> &filePaths,
                              Operation op) {
  if (filePaths.empty())
    return false;

  currentOperation = op;

  if (!OpenClipboard(nullptr))
    return false;

  EmptyClipboard();

  // Calculate size needed for DROPFILES structure + file paths
  size_t totalSize = sizeof(DROPFILES);
  for (const auto &path : filePaths) {
    totalSize += (path.length() + 1) * sizeof(wchar_t);
  }
  totalSize += sizeof(wchar_t); // Final null terminator

  HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, totalSize);
  if (!hGlobal) {
    CloseClipboard();
    return false;
  }

  DROPFILES *df = (DROPFILES *)GlobalLock(hGlobal);
  df->pFiles = sizeof(DROPFILES);
  df->pt.x = 0;
  df->pt.y = 0;
  df->fNC = FALSE;
  df->fWide = TRUE; // Unicode

  wchar_t *fileList = (wchar_t *)((BYTE *)df + sizeof(DROPFILES));
  wchar_t *currentPos = fileList;

  for (const auto &path : filePaths) {
    int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, currentPos,
                                  path.length() + 1);
    currentPos += len;
  }
  *currentPos = L'\0'; // Double null terminator

  GlobalUnlock(hGlobal);

  SetClipboardData(CF_HDROP, hGlobal);

  // Set preferred drop effect (copy or move)
  UINT dropEffect = (op == Operation::Cut) ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
  HGLOBAL hEffect = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
  if (hEffect) {
    DWORD *effect = (DWORD *)GlobalLock(hEffect);
    *effect = dropEffect;
    GlobalUnlock(hEffect);
    SetClipboardData(RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT),
                     hEffect);
  }

  CloseClipboard();
  return true;
}

std::vector<std::string> FileClipboard::getFiles() {
  std::vector<std::string> files;

  if (!OpenClipboard(nullptr))
    return files;

  HANDLE hData = GetClipboardData(CF_HDROP);
  if (hData) {
    HDROP hDrop = (HDROP)GlobalLock(hData);
    if (hDrop) {
      UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

      for (UINT i = 0; i < fileCount; i++) {
        UINT pathLen = DragQueryFileW(hDrop, i, nullptr, 0);
        std::vector<wchar_t> buffer(pathLen + 1);
        DragQueryFileW(hDrop, i, buffer.data(), pathLen + 1);

        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1,
                                             nullptr, 0, nullptr, nullptr);
        std::string path(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, &path[0], sizeNeeded,
                            nullptr, nullptr);
        path.resize(sizeNeeded - 1); // Remove null terminator

        files.push_back(path);
      }

      GlobalUnlock(hData);
    }
  }

  CloseClipboard();
  return files;
}

FileClipboard::Operation FileClipboard::getOperation() {
  if (!OpenClipboard(nullptr))
    return currentOperation;

  UINT format = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
  HANDLE hData = GetClipboardData(format);

  if (hData) {
    DWORD *effect = (DWORD *)GlobalLock(hData);
    if (effect) {
      currentOperation =
          (*effect == DROPEFFECT_MOVE) ? Operation::Cut : Operation::Copy;
      GlobalUnlock(hData);
    }
  }

  CloseClipboard();
  return currentOperation;
}

bool FileClipboard::hasFiles() {
  if (!OpenClipboard(nullptr))
    return false;
  bool has = IsClipboardFormatAvailable(CF_HDROP);
  CloseClipboard();
  return has;
}

bool FileClipboard::clear() {
  if (!OpenClipboard(nullptr))
    return false;
  bool result = EmptyClipboard();
  CloseClipboard();
  return result;
}

#elif __APPLE__
// macOS implementation using Objective-C++

bool FileClipboard::copyFiles(const std::vector<std::string> &filePaths,
                              Operation op) {
  @autoreleasepool {
    if (filePaths.empty())
      return false;

    currentOperation = op;

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];

    NSMutableArray *fileURLs = [NSMutableArray array];
    for (const auto &path : filePaths) {
      NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
      NSURL *url = [NSURL fileURLWithPath:nsPath];
      [fileURLs addObject:url];
    }

    [pasteboard writeObjects:fileURLs];
    return true;
  }
}

std::vector<std::string> FileClipboard::getFiles() {
  std::vector<std::string> files;

  @autoreleasepool {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *classes = @[ [NSURL class] ];
    NSDictionary *options = @{NSPasteboardURLReadingFileURLsOnlyKey : @YES};

    NSArray *fileURLs =
        [pasteboard readObjectsForClasses:classes options:options];

    for (NSURL *url in fileURLs) {
      const char *path = [[url path] UTF8String];
      if (path) {
        files.push_back(path);
      }
    }
  }

  return files;
}

FileClipboard::Operation FileClipboard::getOperation() {
  return currentOperation;
}

bool FileClipboard::hasFiles() {
  @autoreleasepool {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *classes = @[ [NSURL class] ];
    NSDictionary *options = @{NSPasteboardURLReadingFileURLsOnlyKey : @YES};

    return [pasteboard canReadObjectForClasses:classes options:options];
  }
}

bool FileClipboard::clear() {
  @autoreleasepool {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    return true;
  }
}

#elif __linux__
// Linux implementation using xclip

std::string exec(const char *cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe)
    return "";
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

bool FileClipboard::copyFiles(const std::vector<std::string> &filePaths,
                              Operation op) {
  if (filePaths.empty())
    return false;

  currentOperation = op;

  // Build URI list
  std::stringstream uriList;
  for (const auto &path : filePaths) {
    uriList << "file://" << path << "\n";
  }

  // Use xclip to set clipboard
  std::string cmd = "echo -n \"" + uriList.str() +
                    "\" | xclip -selection clipboard -t text/uri-list";
  int result = system(cmd.c_str());

  return result == 0;
}

std::vector<std::string> FileClipboard::getFiles() {
  std::vector<std::string> files;

  std::string output =
      exec("xclip -selection clipboard -t text/uri-list -o 2>/dev/null");

  if (output.empty())
    return files;

  std::istringstream stream(output);
  std::string line;

  while (std::getline(stream, line)) {
    if (line.find("file://") == 0) {
      std::string path = line.substr(7); // Remove "file://"
      // Remove trailing whitespace/newline
      path.erase(
          std::find_if(path.rbegin(), path.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          path.end());

      if (!path.empty()) {
        files.push_back(path);
      }
    }
  }

  return files;
}

FileClipboard::Operation FileClipboard::getOperation() {
  return currentOperation;
}

bool FileClipboard::hasFiles() {
  std::string output =
      exec("xclip -selection clipboard -t TARGETS -o 2>/dev/null");
  return output.find("text/uri-list") != std::string::npos;
}

bool FileClipboard::clear() {
  int result = system("echo -n \"\" | xclip -selection clipboard");
  return result == 0;
}

#endif

} // namespace flux
