# Makefile for fx - Simple wrapper around CMake
# Makes installation easier with sensible defaults

PREFIX ?= /usr/local
BUILD_DIR ?= build
BUILD_TYPE ?= Release

.PHONY: all build install uninstall clean help init-config

all: build

# Build the project
build:
	@echo "==> Building fx..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX) \
		-DFLUX_INSTALL=ON \
		-DBUILD_STANDALONE=ON \
		..
	@cmake --build $(BUILD_DIR) -j$(shell nproc 2>/dev/null || echo 4)
	@echo "==> Build complete! Binary at: $(BUILD_DIR)/fx"

# Install system-wide
install: build
	@echo "==> Installing fx to $(PREFIX)..."
	@cd $(BUILD_DIR) && sudo cmake --install .
	@echo "==> Installation complete!"
	@echo ""
	@echo "Next steps:"
	@echo "  1. Run: fx --init-config"
	@echo "  2. Edit: ~/.config/fx/config.toml"
	@echo "  3. Add themes to: ~/.config/fx/themes/"
	@echo ""

# Install to user directory (no sudo needed)
install-user:
	@echo "==> Installing fx to ~/.local/bin..."
	@$(MAKE) build PREFIX=$$HOME/.local
	@cd $(BUILD_DIR) && cmake --install . --prefix=$$HOME/.local
	@echo "==> User installation complete!"
	@echo ""
	@echo "Make sure ~/.local/bin is in your PATH"
	@echo "Add to your ~/.bashrc or ~/.zshrc:"
	@echo '  export PATH="$$HOME/.local/bin:$$PATH"'
	@echo ""

# Initialize user config
init-config:
	@if command -v fx >/dev/null 2>&1; then \
		fx --init-config; \
	else \
		echo "Error: fx not found in PATH"; \
		echo "Run 'make install' or 'make install-user' first"; \
		exit 1; \
	fi

# Uninstall
uninstall:
	@echo "==> Uninstalling fx from $(PREFIX)..."
	@cd $(BUILD_DIR) && sudo cmake --build . --target uninstall 2>/dev/null || \
		(sudo rm -f $(PREFIX)/bin/fx && \
		 sudo rm -rf $(PREFIX)/share/fx && \
		 echo "Removed fx manually")
	@echo "==> Uninstall complete!"
	@echo ""
	@echo "To remove user config (optional):"
	@echo "  rm -rf ~/.config/fx"

# Clean build artifacts
clean:
	@echo "==> Cleaning build directory..."
	@rm -rf $(BUILD_DIR)
	@echo "==> Clean complete!"

# Development build with debug symbols
debug:
	@$(MAKE) build BUILD_TYPE=Debug
	@echo "==> Debug build complete! Run with gdb:"
	@echo "  gdb $(BUILD_DIR)/fx"

# Run tests (if you add them later)
test: build
	@echo "==> Running tests..."
	@cd $(BUILD_DIR) && ctest --output-on-failure

# Show help
help:
	@echo "fx Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build fx (default)"
	@echo "  make install      - Install system-wide to $(PREFIX)"
	@echo "  make install-user - Install to ~/.local/bin (no sudo)"
	@echo "  make init-config  - Initialize user config at ~/.config/fx"
	@echo "  make uninstall    - Remove system installation"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make help         - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  PREFIX=$(PREFIX)  - Installation prefix"
	@echo "  BUILD_TYPE=$(BUILD_TYPE) - Release or Debug"
	@echo ""
	@echo "Examples:"
	@echo "  make PREFIX=/opt/fx install"
	@echo "  make BUILD_TYPE=Debug build"
	@echo "  make install-user"