CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = test_assign1
SRC = dberror.c storage_mgr.c test_assign1_1.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET) *.o
