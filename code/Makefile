# Root Makefile — Firmware only

.PHONY: all firmware clean size isp flash

# Default target
all: firmware

firmware:
	$(MAKE) -C firmware

size:
	$(MAKE) -C firmware size

isp:
	$(MAKE) -C firmware isp

clean:
	$(MAKE) -C firmware clean

flash:
	$(MAKE) -C firmware flash
