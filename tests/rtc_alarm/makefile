# ------------------------------------------------------------
# Exact one-file build + flash for soft_pwm_demo.c
# Mirrors the manual commands exactly.
# ------------------------------------------------------------

PROJECT := rtc_alarm
MCU     := atmega1284p
F_CPU   := 8000000UL

CC      := avr-g++
OBJCOPY := avr-objcopy
SIZE    := avr-size
AVRDUDE := avrdude

PROGRAMMER  := atmelice_isp
PORT        := usb
AVRDUDE_MCU := m1284p

SRC := rtc_alarm.cpp uart.cpp i2c_avr.cpp mini_printf.cpp

all: $(PROJECT).hex flash

$(PROJECT).elf: $(SRC)
	$(CC) \
	  -mmcu=$(MCU) \
	  -DF_CPU=$(F_CPU) \
	  -Os \
	  -Wall -Wextra \
	  -std=gnu++17 \
	  $(SRC) \
	  -o $(PROJECT).elf

$(PROJECT).hex: $(PROJECT).elf
	$(OBJCOPY) -O ihex -R .eeprom \
	  $(PROJECT).elf \
	  $(PROJECT).hex

size: $(PROJECT).elf
	$(SIZE) --mcu=$(MCU) $(PROJECT).elf

flash: $(PROJECT).hex
	$(AVRDUDE) \
	  -c $(PROGRAMMER) \
	  -P $(PORT) \
	  -p $(AVRDUDE_MCU) \
	  -U flash:w:$(PROJECT).hex:i

clean:
	rm -f $(PROJECT).elf $(PROJECT).hex

.PHONY: all size flash clean
