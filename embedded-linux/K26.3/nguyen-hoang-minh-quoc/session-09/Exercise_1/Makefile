CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2

TARGETS = daemon cli

all: $(TARGETS)

daemon: daemon.c sensor_shm.h
	$(CC) $(CFLAGS) -o $@ daemon.c

cli: cli.c sensor_shm.h
	$(CC) $(CFLAGS) -o $@ cli.c

clean:
	rm -f $(TARGETS)

.PHONY: all clean
