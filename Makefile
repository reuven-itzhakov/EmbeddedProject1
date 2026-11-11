CFLAG = -Wall -g
CC = gcc
BIN_DIR = bin
INC_DIR = include
SRC_DIR = src

$(BIN_DIR)/%: $(SRC_DIR)/%.c
	$(CC) $(CFLAG) $< -o $@

all: $(BIN_DIR)/cpu_sensor $(BIN_DIR)/mem_sensor $(BIN_DIR)/proc_sensor $(BIN_DIR)/sysmond

clean:
	rm -f $(BIN_DIR)/*