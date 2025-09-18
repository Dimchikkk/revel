CC = gcc
CFLAGS = -Wall -g `pkg-config --cflags gtk4 sqlite3 gstreamer-1.0 gstreamer-video-1.0 gstreamer-app-1.0` -lm
LIBS = `pkg-config --libs gtk4 sqlite3 gstreamer-1.0 gstreamer-video-1.0 gstreamer-app-1.0` -lm -luuid

SRCS = main.c canvas_core.c canvas_input.c canvas_actions.c canvas_spaces.c \
       element.c paper_note.c note.c connection.c media_note.c space.c \
       database.c model.c canvas_search.c canvas_space_select.c canvas_drop.c undo_manager.c
OBJS = $(SRCS:.c=.o)
TARGET = revel

# Test configuration
TEST_MODEL_SRCS = test_model.c
TEST_UNDO_SRCS = test_undo_manager.c
TEST_MODEL_OBJS = $(TEST_MODEL_SRCS:.c=.o)
TEST_UNDO_OBJS = $(TEST_UNDO_SRCS:.c=.o)

# Add test files to objects but exclude main.c for test build
TEST_MODEL_OBJS_FULL = $(filter-out main.o, $(OBJS)) $(TEST_MODEL_OBJS)
TEST_UNDO_OBJS_FULL = $(filter-out main.o, $(OBJS)) $(TEST_UNDO_OBJS)

TEST_MODEL_TARGET = test_model_runner
TEST_UNDO_TARGET = test_undo_runner

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test targets
test: test-model test-undo

test-model: $(TEST_MODEL_TARGET)
	./$(TEST_MODEL_TARGET)

test-undo: $(TEST_UNDO_TARGET)
	./$(TEST_UNDO_TARGET)

$(TEST_MODEL_TARGET): $(TEST_MODEL_OBJS_FULL)
	$(CC) -o $@ $(TEST_MODEL_OBJS_FULL) $(LIBS) `pkg-config --libs glib-2.0`

$(TEST_UNDO_TARGET): $(TEST_UNDO_OBJS_FULL)
	$(CC) -o $@ $(TEST_UNDO_OBJS_FULL) $(LIBS) `pkg-config --libs glib-2.0`

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_MODEL_OBJS) $(TEST_UNDO_OBJS) $(TEST_MODEL_TARGET) $(TEST_UNDO_TARGET) revel.db test.db

.PHONY: all clean test test-model test-undo
