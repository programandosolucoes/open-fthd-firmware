/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * OmniVision OV5650 Sensor Register Definitions and Multi-Resolution Tables.
 * Derived from Linux kernel media driver (drivers/media/i2c/ov5650.c).
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#ifndef _OV5650_H
#define _OV5650_H

#include <stdint.h>

#define OV5650_I2C_ADDR             0x36

/* Core Control Registers */
#define OV5650_REG_CHIP_ID_HIGH     0x300a
#define OV5650_REG_CHIP_ID_LOW      0x300b
#define OV5650_CHIP_ID              0x5651

#define OV5650_REG_SYSTEM_CTRL1     0x3008
#define OV5650_REG_AEC_PK_EXPO_HIGH 0x3500
#define OV5650_REG_AEC_PK_EXPO_MID  0x3501
#define OV5650_REG_AEC_PK_EXPO_LOW  0x3502
#define OV5650_REG_AGC_PK_MANUAL    0x3503
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
    OV5650_MODE_5MP_15FPS,          /* 2592x1944 @ 15fps (Native 5MP Full Frame) */
    OV5650_MODE_COUNT
};

struct ov5650_mode_desc {
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    const struct ov5650_reg_val *regs;
};

/* Mode 0: 1280x720 @ 30fps */
static const struct ov5650_reg_val ov5650_regs_720p_30fps[] = {
    { 0x3008, 0x82 }, /* Software reset */
    { 0x3008, 0x42 }, /* Normal mode */
    { 0x3103, 0x03 }, /* System clock divider */
    { 0x3017, 0x7f }, /* MIPI CSI-2 enable */
    { 0x3018, 0xfc }, /* MIPI lane configuration */
    { 0x3618, 0x00 }, /* Analog bias */
    { 0x3612, 0x24 }, /* Charge pump control */
    { 0x3704, 0xa0 }, /* Sensor timing */
    { 0x3708, 0x24 }, /* Line counter */
    { 0x3800, 0x00 }, { 0x3801, 0x00 }, /* H-start: 0 */
    { 0x3802, 0x00 }, { 0x3803, 0x00 }, /* V-start: 0 */
    { 0x3804, 0x0a }, { 0x3805, 0x3f }, /* H-end: 2623 */
    { 0x3806, 0x07 }, { 0x3807, 0xa3 }, /* V-end: 1955 */
    { 0x3808, 0x05 }, { 0x3809, 0x00 }, /* Output width: 1280 */
    { 0x380a, 0x02 }, { 0x380b, 0xd0 }, /* Output height: 720 */
    { 0x380c, 0x07 }, { 0x380d, 0x64 }, /* Total H-size: 1892 */
    { 0x380e, 0x02 }, { 0x380f, 0xee }, /* Total V-size: 750 */
    { 0x3818, 0x81 }, /* Mirror / flip */
    { 0x3820, 0x41 }, /* Format control */
    { 0x3821, 0x07 }, /* ISP format control */
    { 0x0000, 0x00 }
};

/* Mode 1: 1920x1080 @ 30fps (Full HD 16:9 crop) */
static const struct ov5650_reg_val ov5650_regs_1080p_30fps[] = {
    { 0x3008, 0x42 },
    { 0x3103, 0x03 },
    { 0x3800, 0x01 }, { 0x3801, 0x50 }, /* H-start: 336 */
    { 0x3802, 0x01 }, { 0x3803, 0xa0 }, /* V-start: 416 */
    { 0x3804, 0x08 }, { 0x3805, 0xd0 }, /* H-end: 2256 */
    { 0x3806, 0x05 }, { 0x3807, 0xd8 }, /* V-end: 1496 */
    { 0x3808, 0x07 }, { 0x3809, 0x80 }, /* Output width: 1920 */
    { 0x380a, 0x04 }, { 0x380b, 0x38 }, /* Output height: 1080 */
    { 0x380c, 0x09 }, { 0x380d, 0x60 }, /* Total H-size: 2400 */
    { 0x380e, 0x04 }, { 0x380f, 0x60 }, /* Total V-size: 1120 */
    { 0x3820, 0x00 }, { 0x3821, 0x00 }, /* 1:1 direct sampling */
    { 0x0000, 0x00 }
};

/* Mode 2: 640x480 @ 60fps (VGA 4:3 4x4 binned) */
static const struct ov5650_reg_val ov5650_regs_vga_60fps[] = {
    { 0x3008, 0x42 },
    { 0x3103, 0x03 },
    { 0x3800, 0x00 }, { 0x3801, 0x00 },
    { 0x3802, 0x00 }, { 0x3803, 0x00 },
    { 0x3804, 0x0a }, { 0x3805, 0x3f },
    { 0x3806, 0x07 }, { 0x3807, 0xa3 },
    { 0x3808, 0x02 }, { 0x3809, 0x80 }, /* Output width: 640 */
    { 0x380a, 0x01 }, { 0x380b, 0xe0 }, /* Output height: 480 */
    { 0x380c, 0x07 }, { 0x380d, 0x64 },
    { 0x380e, 0x02 }, { 0x380f, 0xee },
    { 0x3820, 0x41 }, { 0x3821, 0x01 }, /* 4x4 binning mode */
    { 0x0000, 0x00 }
};

/* Mode 3: 2592x1944 @ 15fps (Native 5MP Full Frame) */
static const struct ov5650_reg_val ov5650_regs_5mp_15fps[] = {
    { 0x3008, 0x42 },
    { 0x3103, 0x03 },
    { 0x3800, 0x00 }, { 0x3801, 0x00 },
    { 0x3802, 0x00 }, { 0x3803, 0x00 },
    { 0x3804, 0x0a }, { 0x3805, 0x20 }, /* H-end: 2592 */
    { 0x3806, 0x07 }, { 0x3807, 0x98 }, /* V-end: 1944 */
    { 0x3808, 0x0a }, { 0x3809, 0x20 }, /* Output width: 2592 */
    { 0x380a, 0x07 }, { 0x380b, 0x98 }, /* Output height: 1944 */
    { 0x380c, 0x0b }, { 0x380d, 0x00 }, /* Total H-size: 2816 */
    { 0x380e, 0x07 }, { 0x380f, 0xd0 }, /* Total V-size: 2000 */
    { 0x3820, 0x00 }, { 0x3821, 0x00 }, /* Full array active */
    { 0x0000, 0x00 }
};

#endif /* _OV5650_H */
