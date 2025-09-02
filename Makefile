CC = gcc
CFLAGS = -Wall -g `pkg-config --cflags gtk4`
LIBS = `pkg-config --libs gtk4`

SRCS = main.c canvas.c note.c connection.c vector.c
OBJS = $(SRCS:.c=.o)
TARGET = velo2

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
