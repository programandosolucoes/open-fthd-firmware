/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * Firmware Main Entry Point and Handshake Engine.
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#include "fthd_ipc.h"
#include "bcm1570_hw.h"

extern void ipc_init(void);
extern uint32_t ipc_get_channel_table_addr(void);
extern uint32_t ipc_get_num_channels(void);
extern void ipc_terminal_write(const char *msg);
extern void ipc_poll_commands(void);
extern int sensor_ov5650_init(void);

void fthd_main(void)
{
    __asm__ volatile("nop;");
    /* 1. Initialize IPC Channel Tables */
    ipc_init();

    /* 2. Publish IPC channels, queue size, and heap request to host handshake registers */
    MMIO_WRITE32(BCM_REG_ISP_FW_CHAN_CTRL, ipc_get_num_channels());
    MMIO_WRITE32(BCM_REG_ISP_FW_QUEUE_CTRL, 44864); /* 44865 entries (+1 in driver) */
    MMIO_WRITE32(BCM_REG_ISP_FW_HEAP_SIZE, 0x300000); /* 3072 KB */

    /* 3. Log startup message to TERMINAL channel */
    ipc_terminal_write("OpenFTHD Firmware v0.1 (Clean-Room)\n");

    /* 4. Signal Host Wake Interrupt (Stage 1 Handshake: bit 0x10 in ISP_IRQ_STATUS) */
    MMIO_WRITE32(BCM_REG_ISP_IRQ_STATUS, 0x10);

    /* Check if running on physical silicon (host driver sets ISP_REG_40008 / 40004 prior to waking CPU) */
    int is_physical_hw = (MMIO_READ32(BCM_REG_ISP_REG_40008) != 0 ||
                          MMIO_READ32(BCM_REG_ISP_REG_40004) != 0);

    if (is_physical_hw) {
        /* 5. Await Host Driver Stage 2 Handshake (Host clears ISP_FW_CHAN_CTRL and writes 0x10 to ISP_REG_41020) */
        while (MMIO_READ32(BCM_REG_ISP_FW_CHAN_CTRL) != 0 &&
               MMIO_READ32(BCM_REG_ISP_REG_41020) == 0) {
            __asm__ volatile("nop");
        }
        MMIO_WRITE32(BCM_REG_ISP_REG_41020, 0);

        /* 6. Publish Channel Table Address (0x00800000) to host */
        MMIO_WRITE32(BCM_REG_ISP_FW_CHAN_CTRL, ipc_get_channel_table_addr());

        /* 7. Signal Host Second Interrupt (Stage 2: bit 0x10 in ISP_IRQ_STATUS) */
        MMIO_WRITE32(BCM_REG_ISP_IRQ_STATUS, 0x10);

        /* 8. Await Magic Value 0x8042006 from Host and Acknowledge by Clearing to 0 */
        while (MMIO_READ32(BCM_REG_ISP_FW_HEAP_SIZE) != 0x8042006) {
            __asm__ volatile("nop");
        }
        MMIO_WRITE32(BCM_REG_ISP_FW_HEAP_SIZE, 0);
    } else {
        /* Mock / QEMU test harness path */
        MMIO_WRITE32(BCM_REG_ISP_FW_CHAN_CTRL, ipc_get_channel_table_addr());
        MMIO_WRITE32(BCM_REG_ISP_IRQ_STATUS, 0x10);
    }

    /* 9. Initialize Sensor (OV5650) */
    sensor_ov5650_init();

    /* 10. Main execution loop: actively process host commands */
    while (1) {
        /* If magic value arrived asynchronously, acknowledge it */
        if (MMIO_READ32(BCM_REG_ISP_FW_HEAP_SIZE) == 0x8042006) {
            MMIO_WRITE32(BCM_REG_ISP_FW_HEAP_SIZE, 0);
        }
        ipc_poll_commands();
        __asm__ volatile("wfe");
    }
}
