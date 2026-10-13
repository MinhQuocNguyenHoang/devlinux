CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2
LDFLAGS ?= -lrt -lpthread

TARGETS = controller device

all: $(TARGETS)

controller: controller.c
	$(CC) $(CFLAGS) -o $@ controller.c $(LDFLAGS)

device: device.c
	$(CC) $(CFLAGS) -o $@ device.c $(LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
