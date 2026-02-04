
.PHONY: host firmware clean

# Default: build everything
all: host firmware

host:
	$(MAKE) -C host

firmware:
	$(MAKE) -C firmware

clean:
	$(MAKE) -C host clean
	$(MAKE) -C firmware clean

.PHONY: size isp

size:
	$(MAKE) -C firmware size

isp:
	$(MAKE) -C firmware isp
