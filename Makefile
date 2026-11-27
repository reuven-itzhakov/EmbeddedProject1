CC = gcc
BIN_DIR = bin
INC_DIR = inc
SRC_DIR = src
CFLAG = -Wall -Wextra -g -I$(INC_DIR)
LDFLAGS = -lrt


$(BIN_DIR)/%: $(SRC_DIR)/%.c
	$(CC) $(CFLAG) $(LDFLAGS) $< -o $@

all: $(BIN_DIR)/cpu_sensor $(BIN_DIR)/mem_sensor $(BIN_DIR)/proc_sensor $(BIN_DIR)/sysmond

clean:
	rm -f $(BIN_DIR)/*