BUILD_DIR  ?= build
BUILD_TYPE ?= Debug
APP        := $(BUILD_DIR)/gui/qt/pragma-chess

GENERATOR := $(if $(shell command -v ninja),-G Ninja,)

.PHONY: help start build run configure clean deps

help: ## Show available targets
	@grep -E '^[a-z]+:.*## ' $(MAKEFILE_LIST) | awk -F':.*## ' '{printf "  make %-10s %s\n", $$1, $$2}'

start: configure ## Launch the GUI and rebuild/restart it on every source change
	@BUILD_DIR=$(BUILD_DIR) ./scripts/dev-watch.sh

build: configure ## Build the GUI once
	@cmake --build $(BUILD_DIR)

run: build ## Build and launch the GUI (no watching)
	@./$(APP)

configure: $(BUILD_DIR)/CMakeFiles/Makefile.cmake

# Written only when generation succeeds, so a failed configure is retried.
$(BUILD_DIR)/CMakeFiles/Makefile.cmake:
	@if [ -z "$$(ls /usr/lib/*/cmake/Qt6/Qt6Config.cmake /usr/lib/cmake/Qt6/Qt6Config.cmake 2>/dev/null)" ] && ! pkg-config --exists Qt6Widgets 2>/dev/null && [ -z "$$CMAKE_PREFIX_PATH$$Qt6_DIR" ]; then \
		echo "Qt 6 development files not found."; \
		echo "Install them with: make deps   (or set CMAKE_PREFIX_PATH to a Qt 6 installation)"; \
		exit 1; \
	fi
	@cmake -S . -B $(BUILD_DIR) $(GENERATOR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

clean: ## Remove the build directory
	@rm -rf $(BUILD_DIR)

deps: ## Install build dependencies (Debian/Ubuntu)
	sudo apt install build-essential cmake ninja-build qt6-base-dev libqt6sql6-sqlite inotify-tools
