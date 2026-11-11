CFLAG = -Wall -g
CC = gcc

%: %.c
	$(CC) $(CFLAG) $< -o $@

all: sysmond mem_sensor pu_sensor proc_sensor

clean:
	rm -f sysmond mem_sensor pu_sensor proc_sensor