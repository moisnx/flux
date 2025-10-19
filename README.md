# Flux

> A blazingly fast, beautiful terminal file browser built with C++ and ncurses

**Flux** (`fx`) is a modern file browser for the terminal that combines the speed of C++ with gorgeous UI design. It's both a standalone application and an embeddable library for building file browsing into your own TUI applications.

![Version](https://img.shields.io/badge/version-0.1.0-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)

## ✨ Features

### Current Features (v0.1.0)

- ⚡ **Lightning fast** - Instant startup and snappy navigation
- 🎨 **Beautiful themes** - Multiple color schemes with full customization
- 🔤 **Nerd Font icons** - File type icons using Nerd Fonts
- ⌨️ **Vim-like navigation** - hjkl keybindings for efficient browsing
- 📁 **Smart sorting** - Sort by name, size, date, or type
- 👻 **Hidden files** - Toggle visibility with a single key
- 🎯 **Embeddable** - Use as a library in your own applications

### Coming Soon

- 📋 File operations (copy, paste, delete, rename, mkdir)
- ✅ Multi-file selection
- 🔍 Fuzzy search and filtering
- 👁️ File preview pane
- 🌳 Git integration
- 📦 Archive browsing
- 🔖 Bookmarks
- ⚙️ Configuration file support

## 🚀 Quick Start

### Installation

#### From Source

**Prerequisites:**

- CMake 3.16+
- C++20 compatible compiler (GCC 10+, Clang 11+, MSVC 2019+)
- ncursesw (Linux/macOS) or PDCursesMod (Windows)

**Linux/macOS:**

```bash
# Install dependencies
# Ubuntu/Debian:
sudo apt-get install libncursesw5-dev

# macOS:
brew install ncurses

# Build
git clone https://github.com/moisnx/flux.git
cd flux
mkdir build && cd build
cmake ..
make
sudo make install
```

**Windows:**

```bash
# Using MSVC (requires PDCursesMod built separately)
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

### Usage

```bash
# Browse current directory
fx

# Browse specific directory
fx ~/projects

# Use specific theme
fx --theme nord

# ASCII mode (no icons)
fx --no-icons

# Show help
fx --help
```

## ⌨️ Keybindings

| Key                    | Action                      |
| ---------------------- | --------------------------- |
| `j` / `↓`              | Move down                   |
| `k` / `↑`              | Move up                     |
| `h` / `←`              | Go to parent directory      |
| `l` / `→` / `Enter`    | Enter directory / open file |
| `g`                    | Jump to top                 |
| `G`                    | Jump to bottom              |
| `.`                    | Toggle hidden files         |
| `s`                    | Cycle sort mode             |
| `r` / `F5`             | Refresh directory           |
| `q` / `Ctrl+Q` / `Esc` | Quit                        |

## 🎨 Themes

Flux comes with several beautiful built-in themes:

- **Default** - Clean dark theme with blue accents
- **Dracula** - The popular Dracula color scheme
- **Nord** - Arctic, north-bluish color palette
- **Gruvbox Dark** - Retro groove colors
- **Solarized Dark** - Precision colors for machines and people
- **Monokai** - Classic editor theme
- **Tokyo Night** - Modern dark theme
- **Catppuccin Mocha** - Soothing pastel theme
- **One Dark** - Atom's iconic theme

### Custom Themes

Create a TOML file in `~/.config/flux/themes/`:

```toml
# ~/.config/flux/themes/mytheme.toml
name = "My Custom Theme"

[colors]
background = "#1E1E2E"
foreground = "#CDD6F4"
selected = "#313244"
directory = "#89B4FA"
executable = "#A6E3A1"
hidden = "#6C7086"
symlink = "#94E2D5"
parent_dir = "#F5C2E7"
status_bar_bg = "#181825"
status_bar_fg = "#CDD6F4"
status_bar_active = "#89B4FA"
ui_secondary = "#6C7086"
ui_border = "#313244"
ui_error = "#F38BA8"
ui_warning = "#FAB387"
ui_accent = "#CBA6F7"
ui_info = "#89DCEB"
ui_success = "#A6E3A1"
```

Then use it with:

```bash
fx --theme mytheme
```

**Note:** Use `"transparent"` for background to use your terminal's background color.

## 📚 Using as a Library

Flux is designed to be easily embedded in other TUI applications:

```cpp
#include <flux/flux.h>

int main() {
    // Initialize ncurses
    initscr();
    start_color();
    use_default_colors();

    // Create browser and renderer
    flux::Browser browser(".");
    flux::Renderer renderer;

    // Set a theme
    flux::ThemeManager theme_manager;
    auto theme = theme_manager.applyThemeDefinition(
        flux::ThemeManager::getNordThemeDef()
    );
    renderer.setTheme(theme);

    // Main loop
    int ch;
    while ((ch = getch()) != 'q') {
        browser.updateScroll(renderer.getViewportHeight());
        renderer.render(browser);

        switch (ch) {
            case KEY_UP: browser.selectPrevious(); break;
            case KEY_DOWN: browser.selectNext(); break;
            case KEY_LEFT: browser.navigateUp(); break;
            case KEY_RIGHT:
                if (browser.isSelectedDirectory()) {
                    browser.navigateInto(browser.getSelectedIndex());
                }
                break;
        }
    }

    endwin();
    return 0;
}
```

Link against the library:

```cmake
find_package(Flux REQUIRED)
target_link_libraries(your_app PRIVATE Flux::flux)
```

## 🏗️ Project Structure

```
flux/
├── include/flux/          # Public API headers
│   ├── core/             # Core browser logic
│   ├── ui/               # Rendering and theming
│   └── flux.h            # Main header
├── src/                  # Implementation
├── fx/                   # Standalone binary
│   ├── main.cpp
│   ├── theme_loader.cpp
│   └── themes/           # Built-in theme files
└── examples/             # Embedding examples
```

## 🛠️ Building Options

```bash
# Build standalone binary only
cmake .. -DBUILD_STANDALONE=ON -DBUILD_EXAMPLES=OFF

# Build as shared library
cmake .. -DBUILD_SHARED_LIBS=ON

# Debug build with symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

## 🎯 Design Philosophy

Flux is built on these core principles:

1. **Performance First** - C++ speed with optimized rendering
2. **Beautiful by Default** - Gorgeous themes and icons out of the box
3. **Embeddable** - First-class library API for integration
4. **Zero Config** - Works great immediately, customize if you want
5. **Keyboard Driven** - Vim-like efficiency for power users

## 🗺️ Roadmap

### Phase 1: MVP ✅

- [x] Directory browsing
- [x] Vim-like navigation
- [x] Multiple sort modes
- [x] Theme system
- [x] Icon support

### Phase 2: File Operations (In Progress)

- [ ] Copy/paste/delete files
- [ ] Rename and create files/directories
- [ ] Multi-file selection
- [ ] Configuration file support

### Phase 3: Power Features

- [ ] Fuzzy search
- [ ] File preview pane
- [ ] Git status integration
- [ ] Bookmarks
- [ ] Archive browsing

### Phase 4: Advanced

- [ ] Tabs/split panes
- [ ] Bulk rename
- [ ] Trash support
- [ ] Plugin system

## 🤝 Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

### Development Setup

```bash
git clone https://github.com/moisnx/flux.git
cd flux
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

## 📄 License

MIT License - see LICENSE file for details

## 🙏 Acknowledgments

- Inspired by [ranger](https://github.com/ranger/ranger), [lf](https://github.com/gokcehan/lf), and [nnn](https://github.com/jarun/nnn)
- Icons provided by [Nerd Fonts](https://www.nerdfonts.com/)
- Built with [tomlplusplus](https://github.com/marzer/tomlplusplus)

## 📞 Contact

- GitHub: [@moisnx](https://github.com/moisnx)
- Issues: [GitHub Issues](https://github.com/moisnx/flux/issues)

---

**Made with ❤️ and C++**
