# Lightpad - Makefile
# Provides standard targets for building, running, testing, and formatting

.DEFAULT_GOAL := help

# Configuration
BUILD_DIR := build
BINARY_NAME := Lightpad
DEFAULT_BUILD_TYPE ?= Release

# Formatting config
CLANG_FORMAT ?= clang-format
FMT_GLOBS := -name "*.cpp" -o -name "*.c" -o -name "*.h" -o -name "*.hpp"

# Colors for output
BOLD := \033[1m
GREEN := \033[32m
BLUE := \033[34m
YELLOW := \033[33m
RED := \033[31m
RESET := \033[0m

# Help target - shows available commands
.PHONY: help
help:
	@echo "$(BOLD)Lightpad - Build System$(RESET)"
	@echo ""
	@echo "$(BOLD)Available targets:$(RESET)"
	@echo "  $(GREEN)install$(RESET)       - Install dependencies (Ubuntu/Debian)"
	@echo "  $(GREEN)check-deps$(RESET)    - Check if dependencies are installed"
	@echo "  $(GREEN)configure$(RESET)     - Configure build with CMake"
	@echo "  $(GREEN)build$(RESET)         - Build the project"
	@echo "  $(GREEN)debug$(RESET)         - Build with debug symbols"
	@echo "  $(GREEN)release$(RESET)       - Build optimized release version"
	@echo "  $(GREEN)run$(RESET)           - Run the application"
	@echo "  $(GREEN)test$(RESET)          - Run tests via CTest"
	@echo "  $(GREEN)clean$(RESET)         - Clean build directory"
	@echo "  $(GREEN)rebuild$(RESET)       - Clean and build"
	@echo "  $(GREEN)format$(RESET)        - Format C/C++ code (clang-format)"
	@echo "  $(GREEN)format-check$(RESET)  - Verify formatting (CI-friendly)"
	@echo "  $(GREEN)dev$(RESET)           - Install + configure + build"
	@echo "  $(GREEN)all$(RESET)           - Alias for build"
	@echo ""
	@echo "$(BOLD)Examples:$(RESET)"
	@echo "  make install"
	@echo "  make build"
	@echo "  make test"

# Install dependencies (Ubuntu/Debian)
.PHONY: install
install:
	@echo "$(BOLD)$(BLUE)Installing dependencies...$(RESET)"
	@sudo apt-get update
	@sudo apt-get install -y \
		build-essential \
		cmake \
		qtbase5-dev \
		qttools5-dev-tools
	@echo "$(GREEN)✓ Dependencies installed successfully$(RESET)"

# Check dependencies
.PHONY: check-deps
check-deps:
	@echo "$(BOLD)$(BLUE)Checking dependencies...$(RESET)"
	@missing=0; \
	for cmd in cmake g++ qmake; do \
		if ! command -v $$cmd >/dev/null 2>&1; then \
			echo "$(RED)Missing: $$cmd$(RESET)"; \
			missing=1; \
		fi; \
	done; \
	if [ $$missing -eq 0 ]; then \
		echo "$(GREEN)✓ All required tools found$(RESET)"; \
	else \
		exit 1; \
	fi

# Create build directory
build-dir:
	@mkdir -p $(BUILD_DIR)

# Configure build with CMake
.PHONY: configure
configure: build-dir
	@echo "$(BOLD)$(BLUE)Configuring build with CMake...$(RESET)"
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(DEFAULT_BUILD_TYPE) -DBUILD_TESTS=ON
	@echo "$(GREEN)✓ Configuration complete$(RESET)"

# Build the project
.PHONY: build
build: configure
	@echo "$(BOLD)$(BLUE)Building project...$(RESET)"
	@cmake --build $(BUILD_DIR) -- -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "$(GREEN)✓ Build complete$(RESET)"

# Build everything (alias for build)
.PHONY: all
all: build

# Debug build
.PHONY: debug
debug: build-dir
	@echo "$(BOLD)$(BLUE)Configuring debug build...$(RESET)"
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
	@cmake --build $(BUILD_DIR) -- -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "$(GREEN)✓ Debug build complete$(RESET)"

# Release build
.PHONY: release
release: build-dir
	@echo "$(BOLD)$(BLUE)Configuring release build...$(RESET)"
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
	@cmake --build $(BUILD_DIR) -- -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "$(GREEN)✓ Release build complete$(RESET)"

# Run the application
.PHONY: run
run: build
	@echo "$(BOLD)$(BLUE)Running Lightpad...$(RESET)"
	@BIN_PATH="$(BUILD_DIR)/App/$(BINARY_NAME)"; \
	if [ ! -x "$$BIN_PATH" ]; then \
		BIN_PATH="$(BUILD_DIR)/$(BINARY_NAME)"; \
	fi; \
	if [ ! -x "$$BIN_PATH" ]; then \
		echo "$(RED)$(BINARY_NAME) not found in build output$(RESET)"; \
		exit 127; \
	fi; \
	"$$BIN_PATH"

# Run tests
.PHONY: test
test: build
	@echo "$(BOLD)$(BLUE)Running tests...$(RESET)"
	@ctest --test-dir $(BUILD_DIR) --output-on-failure

# Clean build directory
.PHONY: clean
clean:
	@echo "$(BOLD)$(YELLOW)Cleaning build directory...$(RESET)"
	@rm -rf $(BUILD_DIR)
	@echo "$(GREEN)✓ Clean complete$(RESET)"

# Rebuild (clean + build)
.PHONY: rebuild
rebuild: clean build

# Development setup (install + configure + build)
.PHONY: dev
dev: install build
	@echo "$(GREEN)✓ Development environment ready!$(RESET)"

# ---- Formatting ----
.PHONY: format format-check

format:
	@echo "$(BOLD)$(BLUE)Formatting C/C++ files with clang-format...$(RESET)"
	@if command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		find . -type f \( $(FMT_GLOBS) \) -not -path "./$(BUILD_DIR)/*" -print0 \
		| xargs -0 -r $(CLANG_FORMAT) -i --style=file; \
		echo "$(GREEN)✓ C/C++ formatting complete$(RESET)"; \
	else \
		echo "$(RED)clang-format not found. Please install it.$(RESET)"; exit 1; \
	fi

format-check:
	@echo "$(BOLD)$(BLUE)Checking formatting compliance...$(RESET)"
	@FAILED=0; \
	if command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		find . -type f \( $(FMT_GLOBS) \) -not -path "./$(BUILD_DIR)/*" -print0 \
		| xargs -0 -r $(CLANG_FORMAT) --dry-run -Werror --style=file || FAILED=1; \
	else \
		echo "$(RED)clang-format not found. Please install it.$(RESET)"; exit 1; \
	fi; \
	if [ $$FAILED -eq 0 ]; then \
		echo "$(GREEN)✓ All formatting checks passed$(RESET)"; \
	else \
		echo "$(RED)✗ Formatting check failed. Run 'make format' to fix.$(RESET)"; \
		exit 1; \
	fi
