CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2

TARGETS = sender receiver

all: $(TARGETS)

sender: sender.c
	$(CC) $(CFLAGS) -o $@ $<

receiver: receiver.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
