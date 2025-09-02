CC = gcc
CFLAGS = -Wall -g `pkg-config --cflags gtk4` -lm
LIBS = `pkg-config --libs gtk4` -lm

SRCS = main.c canvas.c element.c paper_note.c note.c connection.c vector.c undo_manager.c
OBJS = $(SRCS:.c=.o)
TARGET = velo2

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
