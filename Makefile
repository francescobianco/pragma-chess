BUILD_DIR  ?= build
BUILD_TYPE ?= Debug
APP        := $(BUILD_DIR)/gui/qt/pragma-chess
UNAME      := $(shell uname)

# macOS: the dependencies come from Homebrew (make deps). Its tools go first in
# PATH, ahead of any other cmake or ninja (MacPorts), and CMake is pointed at
# its prefix and at its OpenSSL, which is keg-only (not linked into the prefix).
ifeq ($(UNAME),Darwin)
APP        := $(BUILD_DIR)/gui/qt/pragma-chess.app/Contents/MacOS/pragma-chess
BREW       := $(shell command -v brew)
ifneq ($(BREW),)
BREW_PREFIX := $(shell $(BREW) --prefix)
export PATH := $(BREW_PREFIX)/bin:$(PATH)
CMAKE_ARGS += -DCMAKE_PREFIX_PATH=$(BREW_PREFIX)
ifneq ($(wildcard $(BREW_PREFIX)/opt/openssl@3),)
CMAKE_ARGS += -DOPENSSL_ROOT_DIR=$(BREW_PREFIX)/opt/openssl@3
endif
endif
endif

# A build directory configured by another cmake (MacPorts' 3.24, before
# Homebrew's) is configured again, its fetched dependencies too: an old one
# fetched nothing and built the client without Phone Link and the lobby.
CACHED_CMAKE  := $(shell sed -n 's/^CMAKE_COMMAND:INTERNAL=//p' $(BUILD_DIR)/CMakeCache.txt 2>/dev/null)
CURRENT_CMAKE := $(shell PATH="$(PATH)" command -v cmake)
# By full path: make 3.81 (macOS's) looks commands up in the PATH it started with.
CMAKE := $(or $(CURRENT_CMAKE),cmake)
CTEST := $(or $(shell PATH="$(PATH)" command -v ctest),ctest)
ifneq ($(CACHED_CMAKE),)
ifneq ($(CACHED_CMAKE),$(CURRENT_CMAKE))
$(info $(BUILD_DIR) was configured by $(CACHED_CMAKE): configuring it again with $(CURRENT_CMAKE))
$(shell rm -rf $(BUILD_DIR)/CMakeCache.txt $(BUILD_DIR)/CMakeFiles $(BUILD_DIR)/_deps)
endif
endif

GENERATOR :=$(if $(shell command -v ninja),-G Ninja,)

PREFIX ?= $(HOME)/.local

.PHONY: help start fresh-start build run test install desktop-dev configure clean deps stockfish engine site test-windows-setup

help: ## Show available targets
	@grep -E '^[a-z-]+:.*## ' $(MAKEFILE_LIST) | awk -F':.*## ' '{printf "  make %-12s %s\n", $$1, $$2}'

start: configure engine desktop-dev ## Launch the GUI and rebuild/restart it on every source change
	@BUILD_DIR=$(BUILD_DIR) ./scripts/dev-watch.sh

# The first launch after installing, again at every restart: settings, data
# and the chess folder start empty in build/fresh-home (kept until the next
# launch, to look at what the first run wrote).
fresh-start: configure engine desktop-dev ## Like start, but every launch is a first launch (empty home)
	@BUILD_DIR=$(BUILD_DIR) FRESH_HOME="$(abspath $(BUILD_DIR))/fresh-home" ./scripts/dev-watch.sh

# The Windows installer as a Windows user meets it: built with Inno Setup
# under Wine from packaging/windows, run in a fresh Wine prefix (needs wine).
test-windows-setup: ## Build the Windows installer under Wine and run it, as a first install
	@BUILD_DIR=$(BUILD_DIR) ./scripts/test-windows-setup.sh

build: configure ## Build the GUI once
	@$(CMAKE) --build $(BUILD_DIR)

run: build engine desktop-dev ## Build and launch the GUI (no watching)
	@./$(APP)

test: build ## Build and run the tests
	@$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

configure: $(BUILD_DIR)/CMakeFiles/Makefile.cmake

# Written only when generation succeeds, so a failed configure is retried.
$(BUILD_DIR)/CMakeFiles/Makefile.cmake:
	@if [ -z "$$(ls /usr/lib/*/cmake/Qt6/Qt6Config.cmake /usr/lib/cmake/Qt6/Qt6Config.cmake $(if $(BREW_PREFIX),$(BREW_PREFIX)/lib/cmake/Qt6/Qt6Config.cmake) 2>/dev/null)" ] && ! pkg-config --exists Qt6Widgets 2>/dev/null && [ -z "$$CMAKE_PREFIX_PATH$$Qt6_DIR" ]; then \
		echo "Qt 6 development files not found."; \
		echo "Install them with: make deps   (or set CMAKE_PREFIX_PATH to a Qt 6 installation)"; \
		exit 1; \
	fi
	@$(CMAKE) -S . -B $(BUILD_DIR) $(GENERATOR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_ARGS)

install: build ## Install the app, its menu entry and icon (PREFIX, default ~/.local)
	@$(CMAKE) --install $(BUILD_DIR) --prefix $(PREFIX)
	@-update-desktop-database $(PREFIX)/share/applications 2>/dev/null
	@-gtk-update-icon-cache -q -t $(PREFIX)/share/icons/hicolor 2>/dev/null

# Wayland desktops take the dock icon from an installed .desktop entry.
desktop-dev: ## Show the app icon for the development build (user menu entry)
	@BUILD_DIR=$(BUILD_DIR) ./scripts/install-dev-desktop.sh

stockfish: ## Build the bundled engine (packaging/stockfish.env) next to the development build
	@./scripts/build-stockfish.sh $(BUILD_DIR)/gui/qt/engines

# The engine we ship, built once for start and run: the client looks for it
# first, and without it there is no analysis on a computer with no Stockfish.
engine: $(BUILD_DIR)/gui/qt/engines/stockfish

$(BUILD_DIR)/gui/qt/engines/stockfish: packaging/stockfish.env
	@./scripts/build-stockfish.sh $(BUILD_DIR)/gui/qt/engines

site: ## Generate the web site (docs/) from site/
	@python3 site/build.py

clean: ## Remove the build directory
	@rm -rf $(BUILD_DIR)

deps: ## Install build dependencies (Debian/Ubuntu with apt, macOS with Homebrew)
ifeq ($(UNAME),Darwin)
	@xcode-select -p >/dev/null 2>&1 || { echo "Installing the Xcode Command Line Tools (compiler, make, git)…"; xcode-select --install; exit 1; }
	@command -v brew >/dev/null || { echo "Homebrew is needed: see https://brew.sh"; exit 1; }
	brew install cmake ninja pkgconf qtbase qtsvg qttools yaml-cpp openssl@3 fswatch
else
	sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-svg-dev libqt6sql6-sqlite qt6-tools-dev qt6-l10n-tools qt6-translations-l10n libssl-dev inotify-tools
endif
