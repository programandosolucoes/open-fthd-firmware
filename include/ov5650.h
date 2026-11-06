/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * OmniVision OV5650 Sensor Register Definitions.
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
#define OV5650_REG_SC_CMMN_CHIP_ID  0x300a
#define OV5650_REG_AEC_PK_EXPO_HIGH 0x3500
#define OV5650_REG_AEC_PK_EXPO_MID  0x3501
#define OV5650_REG_AEC_PK_EXPO_LOW  0x3502
#define OV5650_REG_AGC_PK_MANUAL    0x3503
#define OV5650_REG_AEC_AGC_AUTO     0x350a

/* Resolution & Timing Configuration (720p 30fps) */
struct ov5650_reg_val {
    uint16_t reg;
    uint8_t  val;
};

/* Standard 1280x720@30fps Mode Initialization Table */
static const struct ov5650_reg_val ov5650_720p_30fps_init[] = {
    { 0x3008, 0x82 }, /* Software reset */
    { 0x3008, 0x42 }, /* Normal mode */
    { 0x3103, 0x03 }, /* System clock divider */
    { 0x3017, 0x7f }, /* MIPI CSI-2 enable */
    { 0x3018, 0xfc }, /* MIPI lane configuration */
    { 0x3618, 0x00 }, /* Analog bias */
    { 0x3612, 0x24 }, /* Charge pump control */
    { 0x3704, 0xa0 }, /* Sensor timing */
    { 0x3708, 0x24 }, /* Line counter */
    { 0x3800, 0x00 }, /* H-start high */
    { 0x3801, 0x00 }, /* H-start low */
    { 0x3802, 0x00 }, /* V-start high */
    { 0x3803, 0x00 }, /* V-start low */
    { 0x3804, 0x0a }, /* H-end high */
    { 0x3805, 0x3f }, /* H-end low (2623) */
    { 0x3806, 0x07 }, /* V-end high */
    { 0x3807, 0xa3 }, /* V-end low (1955) */
    { 0x3808, 0x05 }, /* Output width high (1280) */
    { 0x3809, 0x00 }, /* Output width low */
    { 0x380a, 0x02 }, /* Output height high (720) */
    { 0x380b, 0xd0 }, /* Output height low */
    { 0x380c, 0x07 }, /* Total H-size high */
    { 0x380d, 0x64 }, /* Total H-size low (1892) */
    { 0x380e, 0x02 }, /* Total V-size high */
    { 0x380f, 0xee }, /* Total V-size low (750) */
    { 0x3818, 0x81 }, /* Mirror / flip */
    { 0x3820, 0x41 }, /* Format control */
    { 0x3821, 0x07 }, /* ISP format control */
    { 0x0000, 0x00 }  /* End of table */
};

#endif /* _OV5650_H */
