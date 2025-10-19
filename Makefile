CC = gcc
PKG_CFLAGS = `pkg-config --cflags gtk4 sqlite3 gstreamer-1.0 gstreamer-video-1.0 gstreamer-app-1.0 json-glib-1.0`
PKG_LIBS = `pkg-config --libs gtk4 sqlite3 gstreamer-1.0 gstreamer-video-1.0 gstreamer-app-1.0 json-glib-1.0`

# Build modes: make (debug) or make RELEASE=1 (optimized production)
DEBUG_FLAGS = -Wall -g
RELEASE_FLAGS = -Wall -O3 -DNDEBUG
CFLAGS = $(if $(RELEASE),$(RELEASE_FLAGS),$(DEBUG_FLAGS)) $(PKG_CFLAGS) -Isrc
LIBS = $(PKG_LIBS) -lm -luuid -lutil

SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build
TEST_BUILD_DIR = $(BUILD_DIR)/tests

SRCS = \
	$(wildcard $(SRC_DIR)/*.c) \
	$(wildcard $(SRC_DIR)/dsl/*.c) \
	$(wildcard $(SRC_DIR)/ai/*.c) \
	$(wildcard $(SRC_DIR)/canvas/*.c) \
	$(wildcard $(SRC_DIR)/elements/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TARGET = revel

# Test configuration
TEST_MODEL_SRC = $(TEST_DIR)/test_model.c
TEST_UNDO_SRC = $(TEST_DIR)/test_undo_manager.c
TEST_SPACE_TREE_SRC = $(TEST_DIR)/test_canvas_space_tree.c
TEST_CANVAS_INPUT_SRC = $(TEST_DIR)/test_canvas_input_events.c
TEST_AI_CONTEXT_SRC = $(TEST_DIR)/test_ai_context.c
TEST_DSL_EXECUTOR_SRC = $(TEST_DIR)/test_dsl_executor.c

TEST_MODEL_OBJ = $(TEST_MODEL_SRC:$(TEST_DIR)/%.c=$(TEST_BUILD_DIR)/%.o)
TEST_UNDO_OBJ = $(TEST_UNDO_SRC:$(TEST_DIR)/%.c=$(TEST_BUILD_DIR)/%.o)
TEST_SPACE_TREE_OBJ = $(TEST_SPACE_TREE_SRC:$(TEST_DIR)/%.c=$(TEST_BUILD_DIR)/%.o)
TEST_CANVAS_INPUT_OBJ = $(TEST_CANVAS_INPUT_SRC:$(TEST_DIR)/%.c=$(TEST_BUILD_DIR)/%.o)
TEST_AI_CONTEXT_OBJ = $(TEST_AI_CONTEXT_SRC:$(TEST_DIR)/%.c=$(TEST_BUILD_DIR)/%.o)
TEST_DSL_EXECUTOR_OBJ = $(TEST_DSL_EXECUTOR_SRC:$(TEST_DIR)/%.c=$(TEST_BUILD_DIR)/%.o)

COMMON_OBJS = $(filter-out $(BUILD_DIR)/main.o,$(OBJS))

TEST_MODEL_OBJS_FULL = $(COMMON_OBJS) $(TEST_MODEL_OBJ)
TEST_UNDO_OBJS_FULL = $(COMMON_OBJS) $(TEST_UNDO_OBJ)
TEST_SPACE_TREE_OBJS_FULL = $(COMMON_OBJS) $(TEST_SPACE_TREE_OBJ)
TEST_CANVAS_INPUT_OBJS_FULL = $(COMMON_OBJS) $(TEST_CANVAS_INPUT_OBJ)
TEST_AI_CONTEXT_OBJS_FULL = $(COMMON_OBJS) $(TEST_AI_CONTEXT_OBJ)
TEST_DSL_EXECUTOR_OBJS_FULL = $(COMMON_OBJS) $(TEST_DSL_EXECUTOR_OBJ)

TEST_MODEL_TARGET = $(TEST_BUILD_DIR)/test_model_runner
TEST_UNDO_TARGET = $(TEST_BUILD_DIR)/test_undo_runner
TEST_SPACE_TREE_TARGET = $(TEST_BUILD_DIR)/test_space_tree_runner
TEST_CANVAS_INPUT_TARGET = $(TEST_BUILD_DIR)/test_canvas_input_runner
TEST_AI_CONTEXT_TARGET = $(TEST_BUILD_DIR)/test_ai_context_runner
TEST_DSL_EXECUTOR_TARGET = $(TEST_BUILD_DIR)/test_dsl_executor_runner

all: $(TARGET)


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Test targets
test: test-model test-undo test-space-tree test-canvas-input test-ai-context test-dsl-executor

test-model: $(TEST_MODEL_TARGET)
	./$(TEST_MODEL_TARGET)

test-undo: $(TEST_UNDO_TARGET)
	./$(TEST_UNDO_TARGET)

test-space-tree: $(TEST_SPACE_TREE_TARGET)
	./$(TEST_SPACE_TREE_TARGET)

$(TEST_MODEL_TARGET): $(TEST_MODEL_OBJS_FULL)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(TEST_MODEL_OBJS_FULL) $(LIBS) `pkg-config --libs glib-2.0`

$(TEST_UNDO_TARGET): $(TEST_UNDO_OBJS_FULL)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(TEST_UNDO_OBJS_FULL) $(LIBS) `pkg-config --libs glib-2.0`

$(TEST_SPACE_TREE_TARGET): $(TEST_SPACE_TREE_OBJS_FULL)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(TEST_SPACE_TREE_OBJS_FULL) $(LIBS) `pkg-config --libs glib-2.0`

test-canvas-input: $(TEST_CANVAS_INPUT_TARGET)
	./$(TEST_CANVAS_INPUT_TARGET)

test-ai-context: $(TEST_AI_CONTEXT_TARGET)
	./$(TEST_AI_CONTEXT_TARGET)

test-dsl-executor: $(TEST_DSL_EXECUTOR_TARGET)
	./$(TEST_DSL_EXECUTOR_TARGET)

$(TEST_CANVAS_INPUT_TARGET): $(TEST_CANVAS_INPUT_OBJS_FULL)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(TEST_CANVAS_INPUT_OBJS_FULL) $(LIBS) `pkg-config --libs glib-2.0`

$(TEST_AI_CONTEXT_TARGET): $(TEST_AI_CONTEXT_OBJS_FULL)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(TEST_AI_CONTEXT_OBJS_FULL) $(LIBS) `pkg-config --libs glib-2.0`

$(TEST_DSL_EXECUTOR_TARGET): $(TEST_DSL_EXECUTOR_OBJS_FULL)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(TEST_DSL_EXECUTOR_OBJS_FULL) $(LIBS) `pkg-config --libs glib-2.0`

clean:
	rm -rf $(BUILD_DIR) $(TARGET) revel.db test.db test_space_tree.db

.PHONY: all clean test test-model test-undo test-space-tree
