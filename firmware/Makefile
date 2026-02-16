# ============================================================
# Chicken Coop Controller Firmware Makefile
# Target: ATmega1284P @ 8 MHz (internal RC)
#
# Layout:
#   firmware/
#     Makefile
#     main_firmware.cpp
#     src/
#     platform/
#
# All objects are built into build/
# No objects are created next to source files.
# ============================================================


# ------------------------------------------------------------
# Project Configuration
# ------------------------------------------------------------

PROJECT_VERSION := "2.8.0"

PROJECT := coop_firmware
MCU     := atmega1284p
F_CPU   := 8000000UL


# ------------------------------------------------------------
# Toolchain
# ------------------------------------------------------------

CXX     := avr-g++
OBJCOPY := avr-objcopy
OBJDUMP := avr-objdump
SIZE    := avr-size
AVRDUDE := avrdude
CPPCHECK := cppcheck
CLANG_TIDY := clang-tidy

# ------------------------------------------------------------
# Programmer Configuration
# ------------------------------------------------------------

PROGRAMMER  := atmelice_isp
PORT        := usb
AVRDUDE_MCU := m1284p

# ISP bitclock (lower if programming becomes unreliable)
ISP_BITCLOCK ?= 5


# ------------------------------------------------------------
# Directories
# ------------------------------------------------------------

OBJ_DIR := build


# ------------------------------------------------------------
# Compiler Flags (C++ only)
# ------------------------------------------------------------

CXXFLAGS := \
	-mmcu=$(MCU) \
	-DF_CPU=$(F_CPU) \
	-Wall -Wextra -Werror \
	-Os \
	-ffunction-sections \
	-fdata-sections \
	-fno-exceptions \
	-fno-rtti \
	-std=gnu++17 \
	-Isrc \
	-DPROJECT_VERSION=\"$(PROJECT_VERSION)\"

LDFLAGS := \
	-mmcu=$(MCU) \
	-Wl,--gc-sections \
	-Wl,-Map=$(PROJECT).map


# ------------------------------------------------------------
# Source Files
# ------------------------------------------------------------

SRCS := \
	main_firmware.cpp \
	src/solar.cpp \
	src/config_common.cpp \
	src/time_dst.cpp \
	src/state_reducer.cpp \
	src/schedule_apply.cpp \
	src/scheduler.cpp \
	src/next_event.cpp \
	src/config_events.cpp \
	src/rtc_common.cpp \
	src/resolve_when.cpp \
	src/devices/devices.cpp \
	src/devices/door_device.cpp \
	src/devices/door_state_machine.cpp \
	src/devices/led_device.cpp \
	src/devices/led_state_machine.cpp \
	src/devices/relay_device.cpp \
	src/console/console.cpp \
	src/console/console_cmds.cpp \
	src/console/console_time.cpp \
	src/console/mini_printf.cpp \
	platform/door_avr.cpp \
	platform/door_lock_avr.cpp \
	platform/relays_avr.cpp \
	platform/i2c_avr.cpp \
	platform/door_led_avr.cpp \
	platform/console_io_avr.cpp \
	platform/config_eeprom.cpp \
	platform/config_sw_avr.cpp \
	platform/system_sleep_avr.cpp \
	platform/rtc.cpp \
	platform/uptime.cpp \
	platform/uart.cpp \
	platform/gpio_avr.cpp

#foo_device is sample code to add a new device
#	src/devices/foo_device.cpp \

# ------------------------------------------------------------
# Object Files (mirrored under build/)
# ------------------------------------------------------------

OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)


# ------------------------------------------------------------
# Default Target
# ------------------------------------------------------------

all: $(PROJECT).hex


# ------------------------------------------------------------
# Compile Rule
# ------------------------------------------------------------

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(CXXFLAGS) -c "$<" -o "$@"


# ------------------------------------------------------------
# Link
# ------------------------------------------------------------

$(PROJECT).elf: $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o "$@"
	$(SIZE) "$@"


# ------------------------------------------------------------
# Output Formats
# ------------------------------------------------------------

$(PROJECT).hex: $(PROJECT).elf
	$(OBJCOPY) -O ihex -R .eeprom "$<" "$@"

$(PROJECT).lst: $(PROJECT).elf
	$(OBJDUMP) -h -S "$<" > "$@"


# ------------------------------------------------------------
# Flash (No Chip Erase)
# Preserves EEPROM
# ------------------------------------------------------------

flash-part: $(PROJECT).hex
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_MCU) \
		-B $(ISP_BITCLOCK) -D \
		-U flash:w:"$<":i


# ------------------------------------------------------------
# Flash (Full Erase)
# ------------------------------------------------------------

flash: $(PROJECT).hex
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_MCU) \
		-B $(ISP_BITCLOCK) \
		-U flash:w:"$<":i


# ------------------------------------------------------------
# Fuse Configuration (LOCKED VALUES)
# ------------------------------------------------------------

LFUSE  := 0xE2
HFUSE  := 0xD1
EFUSE  := 0xFD   # BOD 2.7V

set-fuses:
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_MCU) -B 200 \
		-U lfuse:w:$(LFUSE):m \
		-U hfuse:w:$(HFUSE):m \
		-U efuse:w:$(EFUSE):m

check-fuses:
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_MCU) \
		-U lfuse:r:-:h \
		-U hfuse:r:-:h \
		-U efuse:r:-:h


# ------------------------------------------------------------
# Size
# ------------------------------------------------------------

size: $(PROJECT).elf
	@echo "---- AVR Memory Usage ----"
	@$(SIZE) -C --mcu=$(MCU) "$<"


# ------------------------------------------------------------
# Cleanup
# ------------------------------------------------------------

clean:
	rm -rf $(OBJ_DIR) *.elf *.hex *.lst *.map


	# ------------------------------------------------------------
# Static Analysis
# ------------------------------------------------------------


# All include paths used by analyzer
STATIC_INCLUDES := -Isrc -Iplatform

# ------------------------------------------------------------
# Static Analysis (cppcheck)
# ------------------------------------------------------------

STATIC_INCLUDES := -Isrc -Iplatform

static:
	@echo "---- Running cppcheck ----"
	$(CPPCHECK) \
		--enable=warning,performance,style \
		--std=c++17 \
		--inline-suppr \
		--error-exitcode=1 \
		--quiet \
		$(STATIC_INCLUDES) \
		$(SRCS)


# ------------------------------------------------------------
# Combined
# ------------------------------------------------------------


.PHONY: all clean flash flash-part set-fuses check-fuses size static
