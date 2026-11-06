/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * OmniVision OV5650 Sensor Control Implementation.
 * Synthesized using Linux kernel drivers/media/i2c/ov5650.c
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#include "ov5650.h"
#include "bcm1570_hw.h"

/* Stub I2C write function to BCM1570 I2C controller */
static int bcm1570_i2c_write(uint8_t chip_addr, uint16_t reg, uint8_t val)
{
    /* To be bound to BCM1570 internal I2C peripheral MMIO */
    (void)chip_addr;
    (void)reg;
    (void)val;
    return 0;
}

int sensor_ov5650_init(void)
{
    int i = 0;
    while (ov5650_720p_30fps_init[i].reg != 0x0000) {
        bcm1570_i2c_write(OV5650_I2C_ADDR,
                          ov5650_720p_30fps_init[i].reg,
                          ov5650_720p_30fps_init[i].val);
        i++;
    }
    return 0;
}
