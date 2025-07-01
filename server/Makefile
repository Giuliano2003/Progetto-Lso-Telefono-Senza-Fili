CC = gcc

CFLAGS = -Wall -g

LIBS = -luuid -lcurl

GLIB_FLAGS = $(shell pkg-config --cflags --libs glib-2.0)

TARGET = server.out

SRC = server.c translator.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS) $(GLIB_FLAGS)

all: $(TARGET)

clean:
	rm -f $(TARGET)
	clear