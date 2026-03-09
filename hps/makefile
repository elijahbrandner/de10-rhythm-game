# ------------------------------------------------------------
# Rhythm Game Makefile
# ------------------------------------------------------------

CC = gcc

SRC_DIR = src
INC_DIR = includes
LIB_DIR = lib

OBJ_DIR = obj
BIN_DIR = bin

TARGET = $(BIN_DIR)/rhythm_game

CFLAGS = -std=gnu99 -Wall -Wextra -O2 \
	-I$(INC_DIR) \
	-I$(INC_DIR)/vendor \
	-I$(INC_DIR)/vendor/lcd \
	-I$(INC_DIR)/vendor/socal \
	-I$(LIB_DIR)

LDFLAGS = -lrt

# ------------------------------------------------------------
# Source Files
# ------------------------------------------------------------

SRCS = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/fpga/fpga_if.c \
	$(SRC_DIR)/game/game.c \
	$(SRC_DIR)/game/sequence.c \
	$(SRC_DIR)/game/scoring.c \
	$(SRC_DIR)/game/timing.c \
	$(SRC_DIR)/hal/hal-api.c \
	$(SRC_DIR)/peripherals/button.c \
	$(SRC_DIR)/peripherals/switch.c \
	$(SRC_DIR)/peripherals/lcd.c \
	$(SRC_DIR)/peripherals/accel.c \
	$(SRC_DIR)/vendor/lcd/LCD_Driver.c \
	$(SRC_DIR)/vendor/lcd/LCD_Hw.c \
	$(SRC_DIR)/vendor/lcd/LCD_Lib.c \
	$(SRC_DIR)/vendor/lcd/lcd_graphic.c \
	$(SRC_DIR)/vendor/lcd/font.c \
	$(SRC_DIR)/vendor/lcd/terasic_lib.c

OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# ------------------------------------------------------------
# Default target
# ------------------------------------------------------------

all: directories $(TARGET)

# ------------------------------------------------------------
# Create directories
# ------------------------------------------------------------

directories:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/fpga
	@mkdir -p $(OBJ_DIR)/game
	@mkdir -p $(OBJ_DIR)/hal
	@mkdir -p $(OBJ_DIR)/peripherals
	@mkdir -p $(OBJ_DIR)/vendor/lcd
	@mkdir -p $(BIN_DIR)

# ------------------------------------------------------------
# Link
# ------------------------------------------------------------

$(TARGET): $(OBJS)
	@echo "Linking $@..."
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

# ------------------------------------------------------------
# Compile
# ------------------------------------------------------------

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ------------------------------------------------------------
# Utilities
# ------------------------------------------------------------

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Clean complete."

run: $(TARGET)
	@echo "Running rhythm game..."
	sudo ./$(TARGET)

.PHONY: all clean run directories