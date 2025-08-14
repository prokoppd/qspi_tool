# Compiler and flags
CFLAGS  := -Wall -Wextra -O2 -ggdb -g --pedantic -std=c23
# CFLAGS  := -Wall -Wextra -O2 -ggdb -g -std=c23 -I/opt/efacec-real-time-edge/2.9/sysroots/armv8a-poky-linux/usr/include/
# CFLAGS  := -Wall -Wextra -O3 -Wl,--strip-debug -I/opt/efacec-real-time-edge/2.9/sysroots/armv8a-poky-linux/usr/include/
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

