# OpenFTHD Firmware - Clean-Room Provenance & Source Attribution

## 1. Project Mission & Methodology
The goal of this project is to provide a 100% open-source, clean-room alternative firmware for the Broadcom BCM1570 PCIe Image Signal Processor (FaceTime HD camera found in Apple MacBook Pro/Air models), eliminating the requirement for proprietary extracted binary blobs (`firmware.bin`).

This project follows strict clean-room engineering principles:
- **Zero Proprietary Apple Code**: No binary code, decompiled routines, or copyrighted symbols from Apple Inc. are copied into this codebase.
- **Open Standards & Public Interfaces**: All algorithms, register addresses, and protocols implemented here are synthesized from publicly available documentation, open-source Linux kernel drivers, and interoperability interface definitions.

---

## 2. Public Code & Knowledge Attribution

### A. Linux Kernel Media Subsystem (GPL-2.0)
- **Sensor Driver**: OmniVision OV5650 sensor initialization sequences, clock tree calculations, and register addresses are derived from:
  - Repository: `torvalds/linux`
  - Path: `drivers/media/i2c/ov5650.c`
  - Authors: Guennadi Liakhovetski, Antonio Ospite, Sakari Ailus.
  - License: GNU General Public License v2.0 (GPL-2.0).

- **Actuator Driver**: Auto-focus voice-coil motor (VCM) timing and D/A control:
  - Repository: `torvalds/linux`
  - Paths: `drivers/media/i2c/ad5820.c`, `drivers/media/i2c/dw9714.c`
  - License: GNU General Public License v2.0 (GPL-2.0).

### B. Linux bcwc_pcie Driver (GPL-2.0)
- **Host IPC Protocol & Ringbuffers**: The PCIe communication protocol, channel descriptors, and command opcodes are derived from the public host driver interface:
  - Repository: `https://github.com/patjak/bcwc_pcie`
  - Author: Patrik Jakobsson <patrik.r.jakobsson@gmail.com>
  - Files: `fthd_isp.h`, `fthd_ringbuf.h`, `fthd_reg.h`
  - Protocol Elements:
    - IPC Channels: `TERMINAL`, `DEBUG`, `SHAREDMALLOC`, `IO`, `BUF_H2T`, `BUF_T2H`, `IO_T2H`
    - Commands: `CISP_CMD_START (0x0)`, `CISP_CMD_STOP (0x1)`, `CISP_CMD_RESET (0x2)`, `CISP_CMD_CONFIG_GET (0x3)`, `CISP_CMD_PRINT_ENABLE (0x4)`, `CISP_CMD_POWER_DOWN (0xa)`
    - Handshake Registers: `ISP_FW_CHAN_CTRL (0xc3000)`, `ISP_FW_QUEUE_CTRL (0xc3004)`, `ISP_FW_SIZE (0xc3008)`, `ISP_FW_HEAP_SIZE (0xc300c)`
  - License: GNU General Public License v2.0 (GPL-2.0).

### C. ARM Architecture Reference Manual (Public Standard)
- **Processor Family**: ARMv7-A / ARMv7-R (32-bit Little-Endian Cortex architecture).
- **Boot Specification**:
  - Vector table mapping at base address `0x00000000`.
  - CP15 system control registers (SCTLR, VBAR, ACTLR).
  - Mode switching and exception entry handling.
- **Publisher**: ARM Holdings plc.

---

## 3. License
All original firmware source code authored in this repository is licensed under the **GNU General Public License v2.0 (GPL-2.0-only)** to ensure 100% legal compatibility with the Linux kernel and `bcwc_pcie`.
