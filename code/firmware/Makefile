# ------------------------------------------------------------
# Project
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

# ------------------------------------------------------------
# ISP Bitclock (adjust if programming becomes unreliable)
# ------------------------------------------------------------
ISP_BITCLOCK ?= 5

# ------------------------------------------------------------
# Programmer
# ------------------------------------------------------------
PROGRAMMER  := atmelice_isp
PORT        := usb
AVRDUDE_MCU := m1284p

# ------------------------------------------------------------
# Directories
# ------------------------------------------------------------
OBJ_DIR := build


# ------------------------------------------------------------
# C++ Flags (C++ ONLY — no CFLAGS)
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
	-I"../src" \
	-DPROJECT_VERSION=\"$(PROJECT_VERSION)\"

LDFLAGS := \
	-mmcu=$(MCU) \
	-Wl,--gc-sections \
	-Wl,-Map=$(PROJECT).map

# ------------------------------------------------------------
# Sources
#   NOTE: relative paths only (Make cannot tokenise spaces safely)
# ------------------------------------------------------------
SRCS := \
	main_firmware.cpp \
	../src/solar.cpp \
	../src/config_common.cpp \
	../src/time_dst.cpp \
	../src/state_reducer.cpp \
	../src/schedule_apply.cpp \
	../src/scheduler.cpp \
	../src/next_event.cpp \
	../src/config_events.cpp \
	../src/rtc_common.cpp \
	../src/devices/devices.cpp \
	../src/devices/door_device.cpp \
	../src/devices/door_state_machine.cpp \
	../src/devices/foo_device.cpp \
	../src/devices/led_device.cpp \
	../src/devices/led_state_machine.cpp \
	../src/devices/relay_device.cpp \
	../src/resolve_when.cpp \
	../src/console/console.cpp \
	../src/console/console_cmds.cpp \
	../src/console/console_time.cpp \
	../src/console/mini_printf.cpp \
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



# ------------------------------------------------------------
# Objects (ALL under build/)
#
# NOTE: Some sources live under ../src. We keep object files in build/ only,
# never alongside sources. This avoids accidentally linking stale *.o files
# that may already exist in the source tree.
# ------------------------------------------------------------
OBJS := $(subst $(OBJ_DIR)/../,$(OBJ_DIR)/,$(SRCS:%.cpp=$(OBJ_DIR)/%.o))

# ------------------------------------------------------------
# Default target
# ------------------------------------------------------------
all: $(PROJECT).hex

# ------------------------------------------------------------
# Compile
# ------------------------------------------------------------
# Local sources
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(CXXFLAGS) -c "$<" -o "$@"

# Sources from ../ (shared code)
$(OBJ_DIR)/%.o: ../%.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(CXXFLAGS) -c "$<" -o "$@"

# ------------------------------------------------------------
# Link
# ------------------------------------------------------------
$(PROJECT).elf: $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o "$@"
	$(SIZE) "$@"

# ------------------------------------------------------------
# Output formats
# ------------------------------------------------------------
$(PROJECT).hex: $(PROJECT).elf
	$(OBJCOPY) -O ihex -R .eeprom "$<" "$@"

$(PROJECT).lst: $(PROJECT).elf
	$(OBJDUMP) -h -S "$<" > "$@"

isp:
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_MCU) \
	-B 100  -v

# ------------------------------------------------------------
# Flashing (preserve EEPROM, no chip erase)
# Use for iterative bring-up and debugging
# ------------------------------------------------------------
flash-part: $(PROJECT).hex
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_MCU) \
		-B $(ISP_BITCLOCK) -D \
		-U flash:w:"$<":i

# ------------------------------------------------------------
# Flashing FRESH (full chip erase, EEPROM still preserved)
# Use when changing memory layout or after heavy refactors
# ------------------------------------------------------------
flash: $(PROJECT).hex
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_MCU) \
		-B $(ISP_BITCLOCK) \
		-U flash:w:"$<":i

# ------------------------------------------------------------
# Fuse management (LOCKED VALUES)
# ------------------------------------------------------------
check-fuses:
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_MCU) \
		-U lfuse:r:-:h \
		-U hfuse:r:-:h \
		-U efuse:r:-:h

# ------------------------------------------------------------
# Fuse configuration — LOCKED
#
# MCU: atmega1284p
#
# Clock:
#   - Internal 8 MHz RC oscillator
#   - CKDIV8 disabled
#
# Programming / Debug:
#   - ISP enabled (SPIEN programmed)
#   - JTAG ENABLED via fuse (HFUSE bit 6 = 0)
#     NOTE:
#       PF4–PF7 are JTAG pins at reset.
#       Firmware MUST disable JTAG at runtime
#       if PF4–PF7 are used as GPIO.
#
# Reset / Boot:
#   - Reset enabled
#   - Reset vector: Application (BOOTRST = 1)
#   - No bootloader
#
# Brown-Out Detect (BOD):
#   - Disabled (battery-powered system)
#   - Verified stable in this hardware
#
# EEPROM:
#   - Preserved across chip erase (EESAVE = 1)
#
# VERIFIED WORKING FUSE VALUES (READ FROM SILICON):
#   lfuse = 0xE2
#     CKDIV8   = 1  (clock not divided by 8)
#     CKOUT    = 1  (clock output disabled)
#     SUT1:0   = 10 (recommended start-up for internal RC)
#     CKSEL3:0 = 0010 (internal 8 MHz RC oscillator)

#   hfuse = 0xD1  // Preserve EEPROM
#   hfuse = 0xD9  // Erase EEPROM
#
#
#     RSTDISBL = 1  (reset pin enabled)
#     DWEN     = 1  (debugWIRE disabled)
#     SPIEN    = 0  (ISP programming enabled)
#     WDTON    = 1  (watchdog not always-on)
#     EESAVE   = 0  (EEPROM preserved through chip erase)
#     BOOTSZ1:0= 00 (maximum boot size, unused since no bootloader)
#     BOOTRST  = 1  (reset vector = application)

#   efuse = 0xFB
#     BODLEVEL2:0 = 111 (Brown-Out Detect disabled)
# #
# POLICY:
#   - Fuse values are LOCKED.
#   - DO NOT change fuse values unless hardware or clock source changes.
#   - JTAG disable is handled in firmware via MCUCR (JTD bit).
#
#  Brown-Out Detect (BOD):
#   - ENABLED at 2.7 V
#   - Protects EEPROM and SRAM during slow supply collapse
#   - System is deterministic on brown-out reset
#
#   efuse = 0xFD
#     BODLEVEL2:0 = 101 (2.7 V threshold)

# ------------------------------------------------------------
# Fuse Configuration
# ------------------------------------------------------------

LFUSE  := 0xE2
HFUSE  := 0xD1
# EFUSE = 0xFD  (BOD 2.7V)
EFUSE := 0xFD

set-fuses:
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_MCU) -B 200 \
	-U lfuse:w:$(LFUSE):m \
	-U hfuse:w:$(HFUSE):m \
	-U efuse:w:$(EFUSE):m

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

.PHONY: all clean flash flash-all check-fuses set-fuses size isp
