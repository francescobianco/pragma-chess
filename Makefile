BUILD_DIR  ?= build
BUILD_TYPE ?= Debug
APP        := $(BUILD_DIR)/gui/qt/pragma-chess

GENERATOR := $(if $(shell command -v ninja),-G Ninja,)

PREFIX ?= $(HOME)/.local

.PHONY: help start build run test install desktop-dev configure clean deps

help: ## Show available targets
	@grep -E '^[a-z]+:.*## ' $(MAKEFILE_LIST) | awk -F':.*## ' '{printf "  make %-10s %s\n", $$1, $$2}'

start: configure desktop-dev ## Launch the GUI and rebuild/restart it on every source change
	@BUILD_DIR=$(BUILD_DIR) ./scripts/dev-watch.sh

build: configure ## Build the GUI once
	@cmake --build $(BUILD_DIR)

run: build desktop-dev ## Build and launch the GUI (no watching)
	@./$(APP)

test: build ## Build and run the tests
	@ctest --test-dir $(BUILD_DIR) --output-on-failure

configure: $(BUILD_DIR)/CMakeFiles/Makefile.cmake

# Written only when generation succeeds, so a failed configure is retried.
$(BUILD_DIR)/CMakeFiles/Makefile.cmake:
	@if [ -z "$$(ls /usr/lib/*/cmake/Qt6/Qt6Config.cmake /usr/lib/cmake/Qt6/Qt6Config.cmake 2>/dev/null)" ] && ! pkg-config --exists Qt6Widgets 2>/dev/null && [ -z "$$CMAKE_PREFIX_PATH$$Qt6_DIR" ]; then \
		echo "Qt 6 development files not found."; \
		echo "Install them with: make deps   (or set CMAKE_PREFIX_PATH to a Qt 6 installation)"; \
		exit 1; \
	fi
	@cmake -S . -B $(BUILD_DIR) $(GENERATOR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

install: build ## Install the app, its menu entry and icon (PREFIX, default ~/.local)
	@cmake --install $(BUILD_DIR) --prefix $(PREFIX)
	@-update-desktop-database $(PREFIX)/share/applications 2>/dev/null
	@-gtk-update-icon-cache -q -t $(PREFIX)/share/icons/hicolor 2>/dev/null

# Wayland desktops take the dock icon from an installed .desktop entry.
desktop-dev: ## Show the app icon for the development build (user menu entry)
	@BUILD_DIR=$(BUILD_DIR) ./scripts/install-dev-desktop.sh

clean: ## Remove the build directory
	@rm -rf $(BUILD_DIR)

deps: ## Install build dependencies (Debian/Ubuntu)
	sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-svg-dev libqt6sql6-sqlite qt6-tools-dev qt6-l10n-tools inotify-tools
