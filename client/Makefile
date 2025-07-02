CC = gcc
CFLAGS = -Wall -Wextra -pthread
TARGET = cli
SRC = cli.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	clear
	rm -f $(TARGET)
