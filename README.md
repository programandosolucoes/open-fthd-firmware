# OpenFTHD - Clean-Room Open-Source Firmware for Broadcom BCM1570

OpenFTHD is a 100% open-source, clean-room alternative firmware for the Broadcom BCM1570 PCIe Image Signal Processor (FaceTime HD camera on Apple MacBook Pro / MacBook Air devices).

The objective is to replace the proprietary extracted blob (`firmware.bin`) with a transparent, fully auditable, and redistributable firmware compiled from source under the **GPL-2.0** license.

---

## 1. Architectural Overview

The Broadcom BCM1570 is a PCIe coprocessor featuring an internal ARMv7 core. The host Linux kernel driver (`bcwc_pcie`) loads the raw firmware image into device RAM at physical address `0x00000000` and signals execution via PCIe register `0x40004`.

```
                  ┌──────────────────────────────────────────────┐
                  │           Linux Kernel Driver                │
                  │               (bcwc_pcie)                    │
                  └──────────────────────┬───────────────────────┘
                                         │ PCIe DMA & Ringbuffers
                  ┌──────────────────────▼───────────────────────┐
                  │        Broadcom BCM1570 (ISP)                │
                  │  ┌────────────────────────────────────────┐  │
                  │  │       OpenFTHD Firmware (ARMv7)        │  │
                  │  │  • Vector Table & Reset (boot.S)       │  │
                  │  │  • Handshake & Main Loop (main.c)      │  │
                  │  │  • IPC Channels & Ringbuffers (ipc.c)  │  │
                  │  │  • OmniVision OV5650 Driver (sensor.c) │  │
                  │  └────────────────────────────────────────┘  │
                  └──────────────────────┬───────────────────────┘
                                         │ MIPI CSI-2 & I2C
                               ┌─────────▼────────┐
                               │ OmniVision OV5650│
                               │   Camera Sensor  │
                               └──────────────────┘
```

---

## 2. Clean-Room Provenance & Public Code Sources

This project uses **zero proprietary Apple code** or closed-source decompilations. All components are synthesized from documented public standards and open-source Linux drivers:

1. **OmniVision OV5650 Sensor Control**:
   - Source: Linux kernel upstream [`drivers/media/i2c/ov5650.c`](https://github.com/torvalds/linux/blob/master/drivers/media/i2c/ov5650.c) (GPL-2.0).
   - Clock divider, timing, and 1280x720@30fps resolution registers.
2. **Host IPC Protocol**:
   - Source: Linux [`bcwc_pcie`](https://github.com/patjak/bcwc_pcie) driver (`fthd_isp.h`, `fthd_ringbuf.h`) by Patrik Jakobsson (GPL-2.0).
   - Standard channels: `TERMINAL`, `DEBUG`, `SHAREDMALLOC`, `IO`, `BUF_H2T`, `BUF_T2H`, `IO_T2H`.
3. **ARM Boot & Architecture**:
   - Source: ARM Architecture Reference Manual (ARMv7-A/R).

Full provenance details and legal clean-room statements are documented in:
- [English Technical Datasheet & Provenance Report](docs/DATASHEET_AND_PROVENANCE_REPORT.md)
- [Relatório Técnico de Engenharia Reversa e Proveniência (Português)](docs/RELATORIO_TECNICO_E_PROVENIENCIA_PT.md)
- [`SOURCES.md`](SOURCES.md)

---

## 3. Building and Testing the Firmware

The project provides two independent clean-room implementations:
1. **C Reference Firmware**: Classic bare-metal freestanding C.
2. **Rust Bare-Metal Firmware** (`#![no_std]`): Ultra-safe, memory-leak-free, type-safe MMIO implementation under `rust/`.

### Prerequisites
Install the ARM cross-compiler toolchain, Rust target, and QEMU:
```bash
# ARM Toolchain & QEMU
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi qemu-system-arm python3

# Rust Bare-Metal Target (Optional, for building the Rust firmware from source)
rustup target add armv7a-none-eabi
```

### Compile
```bash
# Build both C and Rust firmwares
make all
```
This generates:
- `build/firmware.bin` (C Reference binary: ~1.7 KB)
- `rust/target/firmware_rust.bin` (Rust bare-metal binary: ~420 bytes)

### Automated QEMU Unit Test
Verify both firmware boot sequences and IPC channel setups in virtual emulation:
```bash
make test
```

---

## 4. Multi-Resolution Support

Unlike Apple's closed firmware which artificially restricted capture to 720p@30fps, OpenFTHD implements full resolution profiles for the OmniVision OV5650:
- **5.0 Megapixel (QSXGA)**: 2592 x 1944 @ 15 fps
- **Full HD 1080p**: 1920 x 1080 @ 30 fps
- **HD 720p**: 1280 x 720 @ 60 fps & 30 fps
- **VGA**: 640 x 480 @ 60 fps

---

## 5. License
GNU General Public License v2.0 (GPL-2.0-only).
