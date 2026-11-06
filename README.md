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

Full provenance details are documented in [`SOURCES.md`](SOURCES.md).

---

## 3. Building the Firmware

### Prerequisites
Install the ARM cross-compiler toolchain:
```bash
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi
```

### Compile
```bash
make
```
This generates:
- `build/open_firmware.elf` (ELF binary with debug symbols)
- `build/firmware.bin` (Raw little-endian ARM binary ready for loading)

---

## 4. License
GNU General Public License v2.0 (GPL-2.0-only).
