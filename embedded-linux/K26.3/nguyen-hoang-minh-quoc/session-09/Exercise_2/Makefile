CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2

TARGETS = writer reader

all: $(TARGETS)

writer: writer.c device_cfg.h
	$(CC) $(CFLAGS) -o $@ writer.c

reader: reader.c device_cfg.h
	$(CC) $(CFLAGS) -o $@ reader.c

clean:
	rm -f $(TARGETS)

.PHONY: all clean
