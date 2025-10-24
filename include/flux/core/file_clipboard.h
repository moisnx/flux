#pragma once

#include <ctime>
// #include <filesystem>
// #include <optional>
#include <string>
#include <vector>

// namespace fs = std::filesystem;

namespace flux {
class FileClipboard {
public:
  enum class Operation { Copy, Cut };
  static bool copyFiles(const std::vector<std::string> &filePaths,
                        Operation op = Operation::Copy);
  static std::vector<std::string> getFiles();

  //   Get the operation type(Copy Or cut)
  static Operation getOperation();

  //   Check if clipboard contains file_paths
  static bool hasFiles();

  //   Clear
  static bool clear();

private:
  static Operation currentOperation;
};

} // namespace flux
