CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror
COVERAGE_FLAGS := -fprofile-arcs -ftest-coverage
PKG_CFLAGS := $(shell pkg-config --cflags libpng16)
PKG_LIBS := $(shell pkg-config --libs libpng16)
TARGET := compattor
SRC_DIR := src
BUILD_DIR := build
TEST_DIR := tests
TEST_TARGET := test_runner

SRCS := $(wildcard $(SRC_DIR)/compattor.cpp) $(SRC_DIR)/main.cpp
LIB_SRCS := $(SRC_DIR)/compattor.cpp
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
LIB_OBJ := $(BUILD_DIR)/compattor.o
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_DIR)/test_%.o,$(TEST_SRCS))
GTEST_LIBS := -lgtest -lgtest_main -lpthread

.PHONY: all clean install uninstall test coverage

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PKG_CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(PKG_LIBS) -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

test: $(BUILD_DIR)/$(TEST_TARGET)
	./$(BUILD_DIR)/$(TEST_TARGET)

$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.cpp $(SRC_DIR)/compattor.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -fprofile-arcs -ftest-coverage $(PKG_CFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR)/compattor_coverage.o: $(SRC_DIR)/compattor.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -fprofile-arcs -ftest-coverage $(PKG_CFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR)/$(TEST_TARGET): $(TEST_OBJS) $(BUILD_DIR)/compattor_coverage.o
	$(CXX) $(CXXFLAGS) -fprofile-arcs -ftest-coverage $^ $(PKG_LIBS) $(GTEST_LIBS) -o $@

coverage: test
	@mkdir -p coverage
	@cd $(BUILD_DIR) && gcov -b compattor_coverage.gcda 2>/dev/null | grep -E "(File|Lines executed)" || true
	@cd $(BUILD_DIR) && for f in *.gcov; do [ -f "$$f" ] && mv "$$f" ../coverage/ 2>/dev/null; done
	@echo "--- Coverage Summary ---"
	@grep -A1 "compattor.cpp" coverage/*.gcov 2>/dev/null | grep "Lines executed" || echo "Coverage report: coverage/"

clean:
	rm -rf $(BUILD_DIR) coverage

install: $(BUILD_DIR)/$(TARGET)
	install -Dm755 $(BUILD_DIR)/$(TARGET) /usr/local/bin/$(TARGET)

uninstall:
	rm -f /usr/local/bin/$(TARGET)
