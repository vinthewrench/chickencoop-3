# ----------------------------------------------------------------------
# Simple UART test for ATmega32U4 - internal 8MHz
# ----------------------------------------------------------------------

MCU       = atmega32u4
F_CPU     = 8000000UL
PROJECT   = test

PROGRAMMER = atmelice_isp
PORT      = usb

CC        = avr-gcc
OBJCOPY   = avr-objcopy
SIZE      = avr-size
AVRDUDE   = avrdude

CFLAGS    = -mmcu=$(MCU) \
            -DF_CPU=$(F_CPU) \
            -Os \
            -Wall -Wextra \
            -ffunction-sections \
            -fdata-sections

LDFLAGS   = -mmcu=$(MCU) \
            -Wl,--gc-sections

all: $(PROJECT).hex

$(PROJECT).elf: $(PROJECT).c
	$(CC) $(CFLAGS) $(PROJECT).c $(LDFLAGS) -o $@

$(PROJECT).hex: $(PROJECT).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	$(SIZE) $@

flash: $(PROJECT).hex
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p m32u4 -B 50 -D \
	           -U flash:w:$<:i

check-fuses:
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p m32u4 \
	           -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h

clean:
	rm -f $(PROJECT).elf $(PROJECT).hex

.PHONY: all flash check-fuses clean
