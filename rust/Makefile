# SPDX-License-Identifier: GPL-2.0-only
# OpenFTHD: Bare-Metal Embedded Rust Firmware Makefile

CARGO   = cargo
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size
OBJDUMP = arm-none-eabi-objdump

TARGET_ELF = target/armv7a-none-eabi/release/open-fthd-rust
TARGET_BIN = target/firmware_rust.bin

all: $(TARGET_BIN)

$(TARGET_ELF): Cargo.toml src/*.rs linker.ld
	@echo "[*] Building bare-metal Rust firmware..."
	$(CARGO) build --release
	$(SIZE) $@

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "[+] Generated clean-room Rust firmware binary: $@"
	@ls -lh $@

test: $(TARGET_BIN)
	python3 ../tests/test_qemu_rust.py

disasm: $(TARGET_ELF)
	$(OBJDUMP) -d $< > target/disasm.txt
	@echo "[+] Disassembly saved to target/disasm.txt"

clean:
	$(CARGO) clean

.PHONY: all clean test disasm
