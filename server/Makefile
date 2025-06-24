CC = gcc

CFLAGS = -Wall -g

LIBS = -luuid

TARGET = server.out

SRC = server.c


$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

all: $(TARGET)

clean:
	rm -f $(TARGET)
	clear