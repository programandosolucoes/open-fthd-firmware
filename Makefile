# SPDX-License-Identifier: GPL-2.0-only
# OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera

CC      = arm-none-eabi-gcc
LD      = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE    = arm-none-eabi-size

CFLAGS  = -mcpu=cortex-a7 -marm -Wall -Wextra -O2 -nostdlib -nostartfiles -ffreestanding -fno-builtin -Iinclude
LDFLAGS = -T linker.ld -nostdlib

SRCS    = src/boot.S src/main.c src/ipc.c src/sensor.c
OBJS    = $(SRCS:.c=.o)
OBJS   := $(OBJS:.S=.o)

TARGET_ELF = build/open_firmware.elf
TARGET_BIN = build/firmware.bin

all: $(TARGET_BIN)

build:
	mkdir -p build

src/boot.o: src/boot.S | build
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_ELF): $(OBJS) linker.ld | build
	$(LD) $(LDFLAGS) $(OBJS) -o $@
	$(SIZE) $@

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "Generated clean-room firmware binary: $@"
	@ls -lh $@

disasm: $(TARGET_ELF)
	$(OBJDUMP) -d $< > build/disasm.txt
	@echo "Disassembly saved to build/disasm.txt"

clean:
	rm -rf build src/*.o

.PHONY: all clean disasm
