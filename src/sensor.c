/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * OmniVision OV5650 Sensor Multi-Resolution Mode Controller.
 * Synthesized using Linux kernel drivers/media/i2c/ov5650.c
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#include "ov5650.h"
#include "bcm1570_hw.h"

static const struct ov5650_mode_desc g_ov5650_modes[OV5650_MODE_COUNT] = {
    [OV5650_MODE_720P_30FPS]  = { .width = 1280, .height = 720,  .fps = 30, .regs = ov5650_regs_720p_30fps },
    [OV5650_MODE_720P_60FPS]  = { .width = 1280, .height = 720,  .fps = 60, .regs = ov5650_regs_720p_30fps },
    [OV5650_MODE_1080P_30FPS] = { .width = 1920, .height = 1080, .fps = 30, .regs = ov5650_regs_1080p_30fps },
    [OV5650_MODE_VGA_60FPS]   = { .width = 640,  .height = 480,  .fps = 60, .regs = ov5650_regs_vga_60fps },
    [OV5650_MODE_5MP_15FPS]   = { .width = 2592, .height = 1944, .fps = 15, .regs = ov5650_regs_5mp_15fps },
};

static uint32_t g_current_mode = OV5650_MODE_720P_30FPS;

/* Stub I2C write function to BCM1570 I2C controller */
static int bcm1570_i2c_write(uint8_t chip_addr, uint16_t reg, uint8_t val)
{
    (void)chip_addr;
    (void)reg;
    (void)val;
    return 0;
}

int sensor_ov5650_set_mode(enum ov5650_mode_id mode)
{
    if (mode >= OV5650_MODE_COUNT)
        return -1;

    const struct ov5650_reg_val *table = g_ov5650_modes[mode].regs;
    int i = 0;
    while (table[i].reg != 0x0000) {
        bcm1570_i2c_write(OV5650_I2C_ADDR, table[i].reg, table[i].val);
        i++;
    }

    g_current_mode = mode;
    return 0;
}

int sensor_ov5650_find_mode(uint32_t width, uint32_t height)
{
    for (int i = 0; i < OV5650_MODE_COUNT; i++) {
        if (g_ov5650_modes[i].width == width && g_ov5650_modes[i].height == height)
            return i;
    }
    return OV5650_MODE_720P_30FPS; /* Default fallback */
}

int sensor_ov5650_init(void)
{
    return sensor_ov5650_set_mode(OV5650_MODE_720P_30FPS);
}
