# SPDX-License-Identifier: GPL-2.0-only
# OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera

export PATH := $(HOME)/.cargo/bin:$(PATH)

CC      = arm-none-eabi-gcc
LD      = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE    = arm-none-eabi-size

CFLAGS  = -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=softfp -marm -Wall -Wextra -O2 -nostdlib -nostartfiles -ffreestanding -fno-builtin -Iinclude
LDFLAGS = -T linker.ld -nostdlib

SRCS    = src/boot.S src/main.c src/ipc.c src/sensor.c src/isp_pipeline.c src/aes.c src/neon_ops.c src/compute.c
OBJS    = $(SRCS:.c=.o)
OBJS   := $(OBJS:.S=.o)

TARGET_ELF = build/open_firmware.elf
TARGET_BIN = build/firmware.bin
RUST_BIN   = rust/target/firmware_rust.bin

all: $(TARGET_BIN) rust tools

tools: tools-c tools-rust

tools-c:
	@echo "=== Building C Tools & libfthd ==="
	@make -C tools/c all

tools-rust:
	@echo "=== Building Rust Tools & fthd-ctl ==="
	@cd tools/rust && cargo build --release

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
	@echo "Generated clean-room C firmware binary: $@"
	@ls -lh $@

rust:
	@echo "=== Building OpenFTHD (Bare-Metal Rust) ==="
	@make -C rust all

disasm: $(TARGET_ELF)
	$(OBJDUMP) -d $< > build/disasm.txt
	@echo "Disassembly saved to build/disasm.txt"

test: $(TARGET_BIN) rust
	@echo "=========================================="
	@echo "1. Running C Reference Firmware QEMU Test:"
	@echo "=========================================="
	python3 tests/test_qemu.py
	@echo ""
	@echo "=========================================="
	@echo "2. Running Rust Bare-Metal Firmware QEMU Test:"
	@echo "=========================================="
	python3 tests/test_qemu_rust.py
	@echo ""
	@echo "=========================================="
	@echo "3. Running Hardware ISP Command Dispatcher Test:"
	@echo "=========================================="
	python3 tests/test_commands.py
	@echo ""
	@echo "=========================================="
	@echo "4. Running NEON SIMD & AES Cryptographic Test:"
	@echo "=========================================="
	python3 tests/test_compute_neon_aes.py

clean:
	rm -rf build src/*.o
	make -C rust clean
	make -C tools/c clean
	cd tools/rust && cargo clean

.PHONY: all clean disasm test rust tools tools-c tools-rust
