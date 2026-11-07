/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * OmniVision OV5650 Sensor Register Definitions and Multi-Resolution Tables.
 * Derived cleanly from Linux kernel media driver (drivers/media/i2c/ov5650.c).
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#ifndef _OV5650_H
#define _OV5650_H

#include <stdint.h>

#define OV5650_I2C_ADDR             0x36

/* Core Control Registers */
#define OV5650_REG_SYSTEM_RESET     0x3008
#define OV5650_REG_CHIP_ID_HIGH     0x300a
#define OV5650_REG_CHIP_ID_LOW      0x300b
#define OV5650_CHIP_ID              0x5651

#define OV5650_REG_PLL_CTRL1        0x3010
#define OV5650_REG_PLL_CTRL2        0x3011
#define OV5650_REG_PLL_CTRL3        0x3012
#define OV5650_REG_MIPI_CTRL        0x3017
#define OV5650_REG_MIPI_LANES       0x3018

#define OV5650_REG_AEC_EXPO_HIGH    0x3500
#define OV5650_REG_AEC_EXPO_MID     0x3501
#define OV5650_REG_AEC_EXPO_LOW     0x3502
#define OV5650_REG_AGC_MANUAL       0x3503
#define OV5650_REG_AEC_AGC_AUTO     0x350a

/* Resolution & Timing Configuration Entry */
struct ov5650_reg_val {
    uint16_t reg;
    uint8_t  val;
};

/* Supported Sensor Resolution Modes */
enum ov5650_mode_id {
    OV5650_MODE_720P_30FPS = 0,     /* 1280x720 @ 30fps (Standard HD) */
    OV5650_MODE_720P_60FPS,         /* 1280x720 @ 60fps (High Frame Rate) */
    OV5650_MODE_1080P_30FPS,        /* 1920x1080 @ 30fps (Full HD 16:9 crop) */
    OV5650_MODE_VGA_60FPS,          /* 640x480 @ 60fps (4:3 binned) */
    OV5650_MODE_5MP_15FPS,          /* 2592x1944 @ 15fps (Native 5MP QSXGA Full Frame) */
    OV5650_MODE_COUNT
};

struct ov5650_mode_desc {
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    const char *name;
    const struct ov5650_reg_val *regs;
};

/* API Declarations */
int sensor_ov5650_init(void);
int sensor_ov5650_set_mode(enum ov5650_mode_id mode);
int sensor_ov5650_find_mode(uint32_t width, uint32_t height);
const struct ov5650_mode_desc *sensor_ov5650_get_mode_desc(enum ov5650_mode_id mode);
int sensor_ov5650_set_exposure(uint32_t lines);
int sensor_ov5650_set_gain(uint16_t gain_val);
int sensor_ov5650_set_framerate(uint32_t fps);
int sensor_ov5650_power_on(void);
int sensor_ov5650_power_off(void);

#endif /* _OV5650_H */
