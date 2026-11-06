/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * Hardware Registers and Memory Map.
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#ifndef _BCM1570_HW_H
#define _BCM1570_HW_H

#include <stdint.h>

/* BCM1570 Mailbox / Handshake Registers (Mapped in host MMIO and internal bus) */
#define BCM_REG_ISP_FW_CHAN_CTRL    0xc3000   /* Channel table offset & count */
#define BCM_REG_ISP_FW_QUEUE_CTRL   0xc3004   /* Queue size & pointer */
#define BCM_REG_ISP_FW_SIZE         0xc3008   /* Firmware size */
#define BCM_REG_ISP_FW_HEAP_SIZE    0xc300c   /* Heap size requested */
#define BCM_REG_ISP_FW_HEAP_ADDR    0xc3010   /* Heap physical address */
#define BCM_REG_ISP_FW_HEAP_SIZE2   0xc3014   /* Secondary heap parameter */
#define BCM_REG_ISP_REG_40004       0x40004   /* Host wakeup pulse */
#define BCM_REG_ISP_REG_40008       0x40008   /* Wakeup control */
#define BCM_REG_ISP_REG_41020       0x41020   /* Second interrupt signal */
#define BCM_REG_ISP_IRQ_STATUS      0x40010   /* Interrupt status register */

/* Helper MMIO macros */
#define MMIO_READ32(addr)           (*(volatile uint32_t *)(addr))
#define MMIO_WRITE32(addr, val)     ((*(volatile uint32_t *)(addr)) = (uint32_t)(val))

#endif /* _BCM1570_HW_H */
