CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2

TARGETS = event_monitor event_client

all: $(TARGETS)

event_monitor: event_monitor.c event_monitor.h
	$(CC) $(CFLAGS) -o $@ event_monitor.c

event_client: event_client.c event_monitor.h
	$(CC) $(CFLAGS) -o $@ event_client.c

clean:
	rm -f $(TARGETS)

.PHONY: all clean
