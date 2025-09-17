CC = gcc
CFLAGS = -Wall -g `pkg-config --cflags gtk4 sqlite3 gstreamer-1.0 gstreamer-video-1.0 gstreamer-app-1.0` -lm
LIBS = `pkg-config --libs gtk4 sqlite3 gstreamer-1.0 gstreamer-video-1.0 gstreamer-app-1.0` -lm -luuid

SRCS = main.c canvas_core.c canvas_input.c canvas_actions.c canvas_spaces.c \
       element.c paper_note.c note.c connection.c media_note.c space.c \
       database.c model.c canvas_search.c canvas_space_select.c canvas_drop.c
OBJS = $(SRCS:.c=.o)
TARGET = revel

# Test configuration
TEST_SRCS = test_model.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_TARGET = test_runner

# Add test files to objects but exclude main.c for test build
TEST_OBJS_FULL = $(filter-out main.o, $(OBJS)) $(TEST_OBJS)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test target
test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS_FULL)
	$(CC) -o $@ $(TEST_OBJS_FULL) $(LIBS) `pkg-config --libs glib-2.0`

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_OBJS) $(TEST_TARGET) revel.db test.db

.PHONY: all clean test
