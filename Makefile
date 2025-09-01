CC = gcc
CFLAGS = -Wall `pkg-config --cflags gtk4 pangocairo`
LIBS = `pkg-config --libs gtk4`

main: main.o
	$(CC) -o main main.o $(LIBS)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f main main.o
