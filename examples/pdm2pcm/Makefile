TARGET := pdm2pcm

CROSS_COMPILE ?= /home/osboxes/buildroot/ma35d1/2024-hmi-sd0/output/host/bin/aarch64-nuvoton-linux-gnu-
CC := $(CROSS_COMPILE)gcc
STRIP := $(CROSS_COMPILE)strip
CFLAGS ?= -O2 -Wall -Wextra
LDFLAGS ?=

all: $(TARGET)

$(TARGET): pdm2pcm.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	-$(STRIP) $@

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
