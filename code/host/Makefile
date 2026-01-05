# ------------------------------------------------------------
# Toolchain
# ------------------------------------------------------------
CXX ?= clang++

# C++ only. No CFLAGS. Ever.
CXXFLAGS := \
	-Wall -Wextra -Werror \
	-O0 -g \
	-std=c++17 \
	-I../src \
	-DPROJECT_VERSION=\"$(PROJECT_VERSION)\" \
	-DHOST_BUILD

# ------------------------------------------------------------
# Directories
# ------------------------------------------------------------
OBJ_DIR := build

# ------------------------------------------------------------
# Source search paths (GNU make 3.81 compatible)
# ------------------------------------------------------------
VPATH := \
	. \
	../src \
	../src/console \
	../src/devices \
	platform

# ------------------------------------------------------------
# Sources (NO ../ and NO src/ prefixes)
# ------------------------------------------------------------
SRCS := \
	main_host.cpp \
	solar.cpp \
	config_common.cpp \
	time_dst.cpp \
	resolve_when.cpp \
	console.cpp \
	console_cmds.cpp \
	console_time.cpp \
	config_events.cpp \
	next_event.cpp \
	scheduler.cpp \
	scheduler_reconcile.cpp \
	mini_printf.cpp \
	devices.cpp \
	door_device.cpp \
	foo_device.cpp \
	relay_device.cpp \
	relay_host.cpp \
	door_hw_host.cpp \
	lock_hw_host.cpp \
	lock.cpp \
	door.cpp \
	console_io_host.cpp \
	config_sw_host.cpp \
	config_host.cpp \
	rtc_host.cpp \
	rtc_common.cpp \
	door_led_host.cpp \
	uptime_host.cpp

# ------------------------------------------------------------
# Objects (all under build/)
# ------------------------------------------------------------
OBJS := $(addprefix $(OBJ_DIR)/,$(SRCS:.cpp=.o))

TARGET := $(OBJ_DIR)/console_host

# ------------------------------------------------------------
# Default target
# ------------------------------------------------------------
all: $(TARGET)

# ------------------------------------------------------------
# Compile rule
# ------------------------------------------------------------
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(CXXFLAGS) -c "$<" -o "$@"

# ------------------------------------------------------------
# Link
# ------------------------------------------------------------
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o "$@"

# ------------------------------------------------------------
# Cleanup
# ------------------------------------------------------------
clean:
	rm -rf $(OBJ_DIR)

.PHONY: all clean
