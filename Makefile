TARGET  := qspi_tool

CFLAGS  := -Wall -Wextra -O2 -ggdb -g --pedantic -std=c2x
LDFLAGS := -lm

SRC_DIR := src
TEST_DIR := test
BUILD_DIR := build


APP_MAIN_OBJ := $(BUILD_DIR)/main.o
APP_BIN := $(BUILD_DIR)/$(TARGET)
APP_SRC := $(wildcard $(SRC_DIR)/*.c)
APP_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(APP_SRC))


UNITY_DIR := libs/unity
UNITY_SRC := $(wildcard $(UNITY_DIR)/*.c)
UNITY_OBJ := $(patsubst $(UNITY_DIR)/%.c,$(BUILD_DIR)/%.o,$(UNITY_SRC))

TEST_BIN := $(BUILD_DIR)/$(TARGET)_test
TEST_SRC := $(wildcard $(TEST_DIR)/*.c) $(filter-out $(APP_MAIN_OBJ), $(APP_OBJ))
TEST_OBJ := $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%.o,$(TEST_SRC))
TEST_OBJ += $(UNITY_OBJ)




all: $(APP_BIN) $(TEST_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ -I$(SRC_DIR) -I$(TEST_DIR) -I$(UNITY_DIR) -DTESTING

$(BUILD_DIR)/%.o: $(UNITY_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ -DUNITY_FIXTURE_NO_EXTRAS -DUNITY_INCLUDE_CONFIG_H -I$(UNITY_DIR) -I$(SRC_DIR) -I$(TEST_DIR)

$(APP_BIN): $(APP_OBJ) | $(BUILD_DIR) 
	$(CC) $(APP_OBJ) -o $@ $(LDFLAGS)

run: $(APP_BIN)
	./$(BIN)

valgrind: $(APP_BIN)
	valgrind --leak-check=full --track-origins=yes ./$(APP_BIN)


$(TEST_BIN): $(TEST_OBJ)
	$(CC) $(TEST_OBJ) -o $@ $(LDFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -I$(UNITY_DIR) -DTEST


test: $(TEST_BIN)
	echo "Running tests..."
	./$(TEST_BIN)

test-valgrind: $(TEST_BIN)
	valgrind --leak-check=full --track-origins=yes ./$(TEST_BIN)
# Clean build files

clean:
	echo "Cleaning build files..."
	rm -rf $(BUILD_DIR)

.PHONY: all clean run valgrind test test-valgrind

