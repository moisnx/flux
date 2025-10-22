#include "include/ui/icon_provider.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace fx {

IconProvider::IconProvider(IconStyle style) : current_style_(style) {
  // Auto-detect if requested
  if (current_style_ == IconStyle::AUTO) {
    current_style_ =
        detectUnicodeSupport() ? IconStyle::NERD_FONTS : IconStyle::ASCII;
  }

  // Initialize Nerd Fonts mappings
  if (current_style_ == IconStyle::NERD_FONTS) {
    initializeNerdFontMap();
  }
}

bool IconProvider::detectUnicodeSupport() const {
#ifdef _WIN32

  // Check if we're in Windows Terminal (best Unicode support)
  const char *wt_session = std::getenv("WT_SESSION");
  if (wt_session) {
    return true;
  }

  // Check for other modern Windows terminals
  const char *term_program = std::getenv("TERM_PROGRAM");
  if (term_program) {
    std::string tp(term_program);
    if (tp.find("vscode") != std::string::npos ||
        tp.find("mintty") != std::string::npos) { // Git Bash uses mintty
      return true;
    }
  }

  // Check ConEmu
  const char *conemuansi = std::getenv("ConEmuANSI");
  if (conemuansi && std::string(conemuansi) == "ON") {
    return true;
  }

  // Check console codepage (65001 = UTF-8)
  const char *codepage = std::getenv("PYTHONIOENCODING");
  if (codepage && strstr(codepage, "utf8")) {
    return true;
  }

  return false;
#else
  // Unix/Linux/macOS detection (original code)

  // Check LANG environment variable for UTF-8
  const char *lang = std::getenv("LANG");
  if (lang &&
      (strstr(lang, "UTF-8") || strstr(lang, "utf8") || strstr(lang, "UTF8"))) {
    return true;
  }

  // Check for modern terminal emulators
  const char *term = std::getenv("TERM");
  if (term) {
    std::string term_str(term);
    if (term_str.find("xterm") != std::string::npos ||
        term_str.find("kitty") != std::string::npos ||
        term_str.find("alacritty") != std::string::npos ||
        term_str.find("wezterm") != std::string::npos ||
        term_str.find("tmux") != std::string::npos ||
        term_str.find("screen") != std::string::npos) {
      return true;
    }
  }

  // Check TERM_PROGRAM for macOS terminals
  const char *term_program = std::getenv("TERM_PROGRAM");
  if (term_program) {
    return true; // Most macOS terminals support Unicode
  }

  return false;
#endif
}
void IconProvider::initializeNerdFontMap() {
  // NOTE: These are Nerd Font codepoints.

  // Programming languages
  nerd_font_map_[".c"] = "\ue61e";   //
  nerd_font_map_[".cpp"] = "\ue61d"; //
  nerd_font_map_[".cc"] = "\ue61d";
  nerd_font_map_[".cxx"] = "\ue61d";
  nerd_font_map_[".h"] = "\ue61e";
  nerd_font_map_[".hpp"] = "\ue61d";
  nerd_font_map_[".hxx"] = "\ue61d";
  nerd_font_map_[".py"] = "\ue606"; //
  nerd_font_map_[".pyc"] = "\ue606";
  nerd_font_map_[".pyo"] = "\ue606";
  nerd_font_map_[".pyd"] = "\ue606";
  nerd_font_map_[".js"] = "\ue74e"; //
  nerd_font_map_[".mjs"] = "\ue74e";
  nerd_font_map_[".cjs"] = "\ue74e";
  nerd_font_map_[".ts"] = "\ue628"; //
  nerd_font_map_[".mts"] = "\ue628";
  nerd_font_map_[".cts"] = "\ue628";
  nerd_font_map_[".jsx"] = "\ue7ba"; //
  nerd_font_map_[".tsx"] = "\ue7ba";
  nerd_font_map_[".rs"] = "\ue7a8";   //
  nerd_font_map_[".go"] = "\ue627";   //
  nerd_font_map_[".java"] = "\ue738"; //
  nerd_font_map_[".class"] = "\ue738";
  nerd_font_map_[".jar"] = "\ue738";
  nerd_font_map_[".rb"] = "\ue791"; //
  nerd_font_map_[".erb"] = "\ue791";
  nerd_font_map_[".php"] = "\ue73d";   //
  nerd_font_map_[".swift"] = "\ue755"; //
  nerd_font_map_[".kt"] = "\ue634";    //
  nerd_font_map_[".kts"] = "\ue634";
  nerd_font_map_[".scala"] = "\ue737"; //
  nerd_font_map_[".lua"] = "\ue620";   //
  nerd_font_map_[".vim"] = "\ue62b";   //
  nerd_font_map_[".sh"] = "\uf489";    //
  nerd_font_map_[".bash"] = "\uf489";
  nerd_font_map_[".zsh"] = "\uf489";
  nerd_font_map_[".fish"] = "\uf489";
  nerd_font_map_[".zig"] = "\ue6a9";  //
  nerd_font_map_[".zon"] = "\ue6a9";  //
  nerd_font_map_[".dart"] = "\ue798"; //
  nerd_font_map_[".ex"] = "\ue62d";   // Elixir
  nerd_font_map_[".exs"] = "\ue62d";
  nerd_font_map_[".erl"] = "\ue7b1"; // Erlang
  nerd_font_map_[".hrl"] = "\ue7b1";
  nerd_font_map_[".clj"] = "\ue768"; // Clojure
  nerd_font_map_[".cljs"] = "\ue768";
  nerd_font_map_[".cljc"] = "\ue768";
  nerd_font_map_[".r"] = "\uf25d"; // R
  nerd_font_map_[".rmd"] = "\uf25d";
  nerd_font_map_[".ml"] = "\ue67a"; // OCaml
  nerd_font_map_[".mli"] = "\ue67a";
  nerd_font_map_[".hs"] = "\ue777"; // Haskell
  nerd_font_map_[".lhs"] = "\ue777";
  nerd_font_map_[".cs"] = "\ue648"; // C#
  nerd_font_map_[".fs"] = "\ue7a7"; // F#
  nerd_font_map_[".fsx"] = "\ue7a7";
  nerd_font_map_[".fsi"] = "\ue7a7";
  nerd_font_map_[".sol"] = "\ue61c"; // Solidity
  nerd_font_map_[".v"] = "\ue6a1";   // Verilog/Coq
  nerd_font_map_[".sv"] = "\ue6a1";
  nerd_font_map_[".vhd"] = "\ue6a1"; // VHDL
  nerd_font_map_[".vhdl"] = "\ue6a1";
  nerd_font_map_[".nim"] = "\ue677"; // Nim
  nerd_font_map_[".pl"] = "\ue769";  // Perl
  nerd_font_map_[".pm"] = "\ue769";

  // Web technologies
  nerd_font_map_[".html"] = "\ue60e"; //
  nerd_font_map_[".htm"] = "\ue60e";
  nerd_font_map_[".css"] = "\ue749";  //
  nerd_font_map_[".scss"] = "\ue603"; //
  nerd_font_map_[".sass"] = "\ue603";
  nerd_font_map_[".less"] = "\ue758"; //
  nerd_font_map_[".json"] = "\ue60b"; //
  nerd_font_map_[".json5"] = "\ue60b";
  nerd_font_map_[".jsonc"] = "\ue60b";
  nerd_font_map_[".xml"] = "\ue619";  //
  nerd_font_map_[".yaml"] = "\uf481"; //
  nerd_font_map_[".yml"] = "\uf481";
  nerd_font_map_[".toml"] = "\ue615";   //
  nerd_font_map_[".vue"] = "\ue6a0";    // Vue
  nerd_font_map_[".svelte"] = "\ue697"; // Svelte

  // Documents
  nerd_font_map_[".md"] = "\ue609"; //
  nerd_font_map_[".markdown"] = "\ue609";
  nerd_font_map_[".txt"] = "\uf15c"; //
  nerd_font_map_[".pdf"] = "\uf1c1"; //
  nerd_font_map_[".doc"] = "\uf1c2"; //
  nerd_font_map_[".docx"] = "\uf1c2";
  nerd_font_map_[".xls"] = "\uf1c3"; //
  nerd_font_map_[".xlsx"] = "\uf1c3";
  nerd_font_map_[".ppt"] = "\uf1c4"; //
  nerd_font_map_[".pptx"] = "\uf1c4";
  nerd_font_map_[".odt"] = "\uf1c2";
  nerd_font_map_[".ods"] = "\uf1c3";
  nerd_font_map_[".odp"] = "\uf1c4";
  nerd_font_map_[".tex"] = "\ue600"; // LaTeX
  nerd_font_map_[".latex"] = "\ue600";

  // Images
  nerd_font_map_[".png"] = "\uf1c5"; //
  nerd_font_map_[".jpg"] = "\uf1c5";
  nerd_font_map_[".jpeg"] = "\uf1c5";
  nerd_font_map_[".gif"] = "\uf1c5";
  nerd_font_map_[".svg"] = "\uf1c5";
  nerd_font_map_[".ico"] = "\uf1c5";
  nerd_font_map_[".bmp"] = "\uf1c5";
  nerd_font_map_[".webp"] = "\uf1c5";
  nerd_font_map_[".tiff"] = "\uf1c5";
  nerd_font_map_[".tif"] = "\uf1c5";

  // Archives
  nerd_font_map_[".zip"] = "\uf410"; //
  nerd_font_map_[".tar"] = "\uf410";
  nerd_font_map_[".gz"] = "\uf410";
  nerd_font_map_[".bz2"] = "\uf410";
  nerd_font_map_[".xz"] = "\uf410";
  nerd_font_map_[".7z"] = "\uf410";
  nerd_font_map_[".rar"] = "\uf410";
  nerd_font_map_[".tgz"] = "\uf410";
  nerd_font_map_[".tbz2"] = "\uf410";
  nerd_font_map_[".txz"] = "\uf410";

  // Media
  nerd_font_map_[".mp3"] = "\uf001"; //
  nerd_font_map_[".mp4"] = "\uf03d"; //
  nerd_font_map_[".avi"] = "\uf03d";
  nerd_font_map_[".mkv"] = "\uf03d";
  nerd_font_map_[".mov"] = "\uf03d";
  nerd_font_map_[".wmv"] = "\uf03d";
  nerd_font_map_[".flv"] = "\uf03d";
  nerd_font_map_[".webm"] = "\uf03d";
  nerd_font_map_[".wav"] = "\uf001";
  nerd_font_map_[".flac"] = "\uf001";
  nerd_font_map_[".ogg"] = "\uf001";
  nerd_font_map_[".m4a"] = "\uf001";
  nerd_font_map_[".aac"] = "\uf001";

  // Git
  nerd_font_map_[".git"] = "\ue702"; //
  nerd_font_map_[".gitignore"] = "\ue702";
  nerd_font_map_[".gitmodules"] = "\ue702";
  nerd_font_map_[".gitattributes"] = "\ue702";
  nerd_font_map_[".gitkeep"] = "\ue702";

  // Config files
  nerd_font_map_[".conf"] = "\ue615"; //
  nerd_font_map_[".config"] = "\ue615";
  nerd_font_map_[".ini"] = "\ue615";
  nerd_font_map_[".env"] = "\uf462"; //
  nerd_font_map_[".editorconfig"] = "\ue615";
  nerd_font_map_[".eslintrc"] = "\ue60b";
  nerd_font_map_[".prettierrc"] = "\ue60b";
  nerd_font_map_[".babelrc"] = "\ue60b";

  // Build files
  nerd_font_map_["makefile"] = "\ue779"; //
  nerd_font_map_["Makefile"] = "\ue779";
  nerd_font_map_["GNUmakefile"] = "\ue779";
  nerd_font_map_["CMakeLists.txt"] = "\ue615";
  nerd_font_map_[".cmake"] = "\ue615";
  nerd_font_map_["package.json"] = "\ue71e"; //
  nerd_font_map_["package-lock.json"] = "\ue71e";
  nerd_font_map_["yarn.lock"] = "\ue6a7"; //
  nerd_font_map_["pnpm-lock.yaml"] = "\ue71e";
  nerd_font_map_["Cargo.toml"] = "\ue7a8"; //
  nerd_font_map_["Cargo.lock"] = "\ue7a8";
  nerd_font_map_["Gemfile"] = "\ue791"; //
  nerd_font_map_["Gemfile.lock"] = "\ue791";
  nerd_font_map_["Rakefile"] = "\ue791";
  nerd_font_map_["build.gradle"] = "\ue738";
  nerd_font_map_["pom.xml"] = "\ue738";
  nerd_font_map_["requirements.txt"] = "\ue606";
  nerd_font_map_["Pipfile"] = "\ue606";
  nerd_font_map_["pyproject.toml"] = "\ue606";
  nerd_font_map_["setup.py"] = "\ue606";
  nerd_font_map_["go.mod"] = "\ue627";
  nerd_font_map_["go.sum"] = "\ue627";

  // Special files
  nerd_font_map_["README"] = "\ue609"; //
  nerd_font_map_["README.md"] = "\ue609";
  nerd_font_map_["readme.md"] = "\ue609";
  nerd_font_map_["LICENSE"] = "\uf48a"; //
  nerd_font_map_["LICENSE.txt"] = "\uf48a";
  nerd_font_map_["COPYING"] = "\uf48a";
  nerd_font_map_["Dockerfile"] = "\uf308"; //
  nerd_font_map_["dockerfile"] = "\uf308";
  nerd_font_map_[".dockerignore"] = "\uf308";
  nerd_font_map_["docker-compose.yml"] = "\uf308";
  nerd_font_map_["docker-compose.yaml"] = "\uf308";
  nerd_font_map_[".vimrc"] = "\ue62b";
  nerd_font_map_[".bashrc"] = "\uf489";
  nerd_font_map_[".zshrc"] = "\uf489";
  nerd_font_map_[".profile"] = "\uf489";
  nerd_font_map_[".bash_profile"] = "\uf489";
  nerd_font_map_["CHANGELOG"] = "\ue609";
  nerd_font_map_["CHANGELOG.md"] = "\ue609";
  nerd_font_map_["CONTRIBUTING"] = "\ue609";
  nerd_font_map_["CONTRIBUTING.md"] = "\ue609";

  // Database
  nerd_font_map_[".sql"] = "\uf472"; //
  nerd_font_map_[".db"] = "\uf472";
  nerd_font_map_[".sqlite"] = "\uf472";
  nerd_font_map_[".sqlite3"] = "\uf472";

  // Font files
  nerd_font_map_[".ttf"] = "\uf031"; //
  nerd_font_map_[".otf"] = "\uf031";
  nerd_font_map_[".woff"] = "\uf031";
  nerd_font_map_[".woff2"] = "\uf031";
  nerd_font_map_[".eot"] = "\uf031";

  // Binary/Executable
  nerd_font_map_[".exe"] = "\uf489"; //
  nerd_font_map_[".dll"] = "\uf489";
  nerd_font_map_[".so"] = "\uf489";
  nerd_font_map_[".dylib"] = "\uf489";
  nerd_font_map_[".app"] = "\uf489";
  nerd_font_map_[".deb"] = "\uf489";
  nerd_font_map_[".rpm"] = "\uf489";
  nerd_font_map_[".apk"] = "\uf489";

  // Certificates/Keys
  nerd_font_map_[".pem"] = "\uf43d"; //
  nerd_font_map_[".crt"] = "\uf43d";
  nerd_font_map_[".cer"] = "\uf43d";
  nerd_font_map_[".key"] = "\uf43d";
  nerd_font_map_[".pub"] = "\uf43d";

  // Log files
  nerd_font_map_[".log"] = "\uf15c"; //

  // Temporary/Backup files
  nerd_font_map_[".tmp"] = "\uf15c";
  nerd_font_map_[".temp"] = "\uf15c";
  nerd_font_map_[".bak"] = "\uf15c";
  nerd_font_map_[".swp"] = "\uf15c";
  nerd_font_map_[".swo"] = "\uf15c";
}

std::string IconProvider::getFileExtension(const std::string &filename) const {
  size_t dot_pos = filename.rfind('.');
  if (dot_pos != std::string::npos && dot_pos > 0) {
    return filename.substr(dot_pos);
  }
  return "";
}

std::string IconProvider::getDirectoryIcon() const {
  return (current_style_ == IconStyle::NERD_FONTS) ? "\uf07c" : "+"; //
}

std::string IconProvider::getParentIcon() const {
  return (current_style_ == IconStyle::NERD_FONTS) ? "\uf0a9" : "^"; //
}

std::string IconProvider::getExecutableIcon() const {
  return (current_style_ == IconStyle::NERD_FONTS) ? "\uf489" : "*"; //
}

std::string IconProvider::getSymlinkIcon() const {
  return (current_style_ == IconStyle::NERD_FONTS) ? "\uf0c1" : "@"; //
}

std::string IconProvider::getHiddenIcon() const {
  return (current_style_ == IconStyle::NERD_FONTS) ? "\uf070" : "."; //
}

std::string IconProvider::getFileIcon(const std::string &filename) const {
  if (current_style_ == IconStyle::ASCII) {
    return " ";
  }

  // Check for exact filename matches first (case-sensitive for special files)
  auto it = nerd_font_map_.find(filename);
  if (it != nerd_font_map_.end()) {
    return it->second;
  }

  // Check for case-insensitive exact matches
  std::string lower_filename = filename;
  std::transform(lower_filename.begin(), lower_filename.end(),
                 lower_filename.begin(), ::tolower);

  it = nerd_font_map_.find(lower_filename);
  if (it != nerd_font_map_.end()) {
    return it->second;
  }

  // Check by extension
  std::string ext = getFileExtension(filename);
  if (!ext.empty()) {
    // Try lowercase extension
    std::string lower_ext = ext;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(),
                   ::tolower);

    it = nerd_font_map_.find(lower_ext);
    if (it != nerd_font_map_.end()) {
      return it->second;
    }
  }

  // Default file icon
  return "\uf15b"; //
}

} // namespace fx