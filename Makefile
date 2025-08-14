# Compiler and flags
CFLAGS  := -Wall -Wextra -O2 -ggdb -g --pedantic -std=c23
TARGET  := qspi_tool
SRC     := main.c qspi.c slog.c

# Default build target
all: $(TARGET)

# Link the program
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

# Clean build files
clean:
	rm -f $(TARGET)

.PHONY: all clean

