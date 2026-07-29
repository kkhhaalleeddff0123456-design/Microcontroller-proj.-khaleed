CC      = C:/WinAVR-20100110/bin/avr-gcc.exe
OBJCOPY = C:/WinAVR-20100110/bin/avr-objcopy.exe
AVRDUDE = C:/WinAVR-20100110/bin/avrdude.exe

MCU     = m32
F_CPU   = 8000000UL
CFLAGS  = -mmcu=atmega32 -DF_CPU=$(F_CPU) -std=c99 -Wall -Os
LDFLAGS = -mmcu=atmega32

# Auto-discover sources
C_SOURCES := \
    $(wildcard Project_04_Vehicle_Dashboard/*.c) \
    $(wildcard MCL/*.c) \
    $(wildcard MCL/*/*.c) \
    $(wildcard MCL/*/*/*.c) \
    $(wildcard HAL/*.c) \
    $(wildcard HAL/*/*.c) \
    $(wildcard HAL/*/*/*.c)

OBJS   := $(patsubst %.c,build/%.o,$(C_SOURCES))
TARGET := build/firmware

# Auto include folders (for #include "gpio.h" in MCL/GPIO/)
INCLUDE_DIRS := Project_04_Vehicle_Dashboard Service \
    $(sort $(dir $(wildcard MCL/*/*.h)) $(wildcard MCL/*/*/*.h)) \
    $(sort $(dir $(wildcard HAL/*/*.h)) $(wildcard HAL/*/*/*.h))
CFLAGS += $(addprefix -I,$(INCLUDE_DIRS))

all: $(TARGET).hex

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

# Direct single-step compile: .c -> .o
build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build

flash: $(TARGET).hex
	$(AVRDUDE) -c usbasp -p $(MCU) -U flash:w:$<:i

.PHONY: all clean flash