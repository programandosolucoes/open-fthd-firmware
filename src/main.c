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
    /* 1. Initialize IPC Channel Tables */
    ipc_init();

    /* 2. Publish IPC channels and queue size to host handshake registers */
    MMIO_WRITE32(BCM_REG_ISP_FW_CHAN_CTRL, ipc_get_num_channels());
    MMIO_WRITE32(BCM_REG_ISP_FW_QUEUE_CTRL, 64); /* 64 queue entries */

    /* 3. Request 2MB of DDR heap memory from host driver */
    MMIO_WRITE32(BCM_REG_ISP_FW_HEAP_SIZE, 0x200000);

    /* 4. Log startup message to TERMINAL channel */
    ipc_terminal_write("OpenFTHD Firmware v0.1-alpha (Clean-Room)\n");

    /* 5. Initialize Sensor (OV5650) */
    sensor_ov5650_init();

    /* 6. Main execution loop: actively process host commands */
    while (1) {
        ipc_poll_commands();
        __asm__ volatile("wfe");
    }
}
