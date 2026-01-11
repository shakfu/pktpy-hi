# Makefile - Frontend for CMake build system
#
# Usage:
#   make          - Build all targets
#   make test     - Run all tests
#   make clean    - Clean build directory
#   make rebuild  - Clean and rebuild

BUILD_DIR := build
CMAKE := cmake
CTEST := ctest

.PHONY: all build test clean rebuild configure help

# Default target
all: build

# Configure CMake (creates build directory)
configure:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) ..

# Build all targets
build: configure
	@$(CMAKE) --build $(BUILD_DIR)

# Run tests (alias for CTest)
test: build
	@cd $(BUILD_DIR) && $(CTEST) --output-on-failure

# Verbose test output
test-verbose: build
	@cd $(BUILD_DIR) && $(CTEST) --verbose

# Clean build directory
clean:
	@rm -rf $(BUILD_DIR)

# Clean and rebuild
rebuild: clean build

# Debug build
debug:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_BUILD_TYPE=Debug ..
	@$(CMAKE) --build $(BUILD_DIR)

# Release build
release:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_BUILD_TYPE=Release ..
	@$(CMAKE) --build $(BUILD_DIR)

# Run a specific test (usage: make run-test TEST=test_scope)
run-test: build
	@cd $(BUILD_DIR) && $(CTEST) -R $(TEST) --verbose

# Run the basic_usage example
example: build
	@./$(BUILD_DIR)/basic_usage

# Help
help:
	@echo "Available targets:"
	@echo "  all          - Build all targets (default)"
	@echo "  build        - Build all targets"
	@echo "  test         - Run all tests"
	@echo "  test-verbose - Run all tests with verbose output"
	@echo "  clean        - Remove build directory"
	@echo "  rebuild      - Clean and rebuild"
	@echo "  debug        - Build with debug flags"
	@echo "  release      - Build with release flags"
	@echo "  run-test     - Run specific test (TEST=name)"
	@echo "  example      - Run basic_usage example"
	@echo "  help         - Show this help message"
