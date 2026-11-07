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

/* BCM1570 Internal Peripheral MMIO Base Address on ARM Cortex-A7 Bus */
#define BCM1570_MMIO_BASE           0xc0000000

/* BCM1570 Mailbox / Handshake Registers */
#define BCM_REG_ISP_FW_CHAN_CTRL    (BCM1570_MMIO_BASE + 0xc3000)   /* Channel table offset & count */
#define BCM_REG_ISP_FW_QUEUE_CTRL   (BCM1570_MMIO_BASE + 0xc3004)   /* Queue size & pointer */
#define BCM_REG_ISP_FW_SIZE         (BCM1570_MMIO_BASE + 0xc3008)   /* Firmware size */
#define BCM_REG_ISP_FW_HEAP_SIZE    (BCM1570_MMIO_BASE + 0xc300c)   /* Heap size requested */
#define BCM_REG_ISP_FW_HEAP_ADDR    (BCM1570_MMIO_BASE + 0xc3010)   /* Heap physical address */
#define BCM_REG_ISP_FW_HEAP_SIZE2   (BCM1570_MMIO_BASE + 0xc3014)   /* Secondary heap parameter */
#define BCM_REG_ISP_REG_40004       (BCM1570_MMIO_BASE + 0x40004)   /* Host wakeup pulse */
#define BCM_REG_ISP_REG_40008       (BCM1570_MMIO_BASE + 0x40008)   /* Wakeup control */
#define BCM_REG_ISP_IRQ_STATUS      (BCM1570_MMIO_BASE + 0x41000)   /* Interrupt status register (bits 0xf0) */
#define BCM_REG_ISP_IRQ_ENABLE      (BCM1570_MMIO_BASE + 0x41004)   /* Interrupt enable */
#define BCM_REG_ISP_REG_41020       (BCM1570_MMIO_BASE + 0x41020)   /* Second interrupt signal / doorbell */
#define BCM_REG_ISP_IRQ_CLEAR       (BCM1570_MMIO_BASE + 0x41024)   /* Interrupt clear register */

/* Helper MMIO macros */
#define MMIO_READ32(addr)           (*(volatile uint32_t *)(addr))
#define MMIO_WRITE32(addr, val)     ((*(volatile uint32_t *)(addr)) = (uint32_t)(val))

#endif /* _BCM1570_HW_H */
