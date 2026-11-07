/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * OmniVision OV5650 Sensor Multi-Resolution Mode Controller.
 * Synthesized cleanly from Linux kernel drivers/media/i2c/ov5650.c
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#include "ov5650.h"
#include "fthd_isp_pipeline.h"
#include "bcm1570_hw.h"

/* Sensor Default Setup Sequence (System PLL, MIPI CSI-2, Analog core) */
static const struct ov5650_reg_val sensor_default_regs[] = {
    { 0x3008, 0x82 }, /* Soft reset */
    { 0x3008, 0x42 }, /* Normal mode */
    { 0x3030, 0x10 }, /* Bypass internal DVDD LDO */
    { 0x3103, 0x93 }, /* System clock divider */
    { 0x3b07, 0x0c },
    { 0x3017, 0xff }, /* MIPI CSI-2 output enable */
    { 0x3018, 0xfc }, /* 2 data lanes active */
    { 0x3706, 0x41 },
    { 0x3630, 0x22 },
    { 0x3605, 0x04 },
    { 0x3606, 0x3f },
    { 0x3712, 0x13 },
    { 0x370e, 0x00 },
    { 0x370b, 0x40 },
    { 0x3600, 0x54 },
    { 0x3601, 0x05 },
    { 0x3713, 0x22 },
    { 0x3714, 0x27 },
    { 0x3631, 0x22 },
    { 0x3612, 0x1a },
    { 0x3604, 0x40 },
    { 0x3710, 0x28 },
    { 0x3702, 0x3a },
    { 0x3704, 0x18 },
    { 0x3a18, 0x00 },
    { 0x3a19, 0xf8 },
    { 0x3a00, 0x38 },
    { 0x380c, 0x0c },
    { 0x380d, 0xb4 },
    { 0x380e, 0x07 },
    { 0x380f, 0xb0 },
    { 0x3830, 0x50 },
    { 0x3a08, 0x12 },
    { 0x3a09, 0x70 },
    { 0x3a0a, 0x0f },
    { 0x3a0b, 0x60 },
    { 0x3a0d, 0x06 },
    { 0x3a0e, 0x06 },
    { 0x3a13, 0x54 },
    { 0x3815, 0x82 },
    { 0x5059, 0x80 },
    { 0x3615, 0x52 },
    { 0x505a, 0x0a },
    { 0x505b, 0x2e },
    { 0x3826, 0x00 },
    { 0x3a1a, 0x06 },
    { 0x3623, 0x01 },
    { 0x3633, 0x24 },
    { 0x3c01, 0x34 },
    { 0x3c04, 0x28 },
    { 0x0000, 0x00 }
};

/* Mode 0: 1280x720 @ 30fps (48MHz PCLK) */
static const struct ov5650_reg_val ov5650_regs_720p_30fps[] = {
    { 0x3613, 0xc4 },
    { 0x370d, 0x42 },
    { 0x3703, 0x9a },
    { 0x3705, 0xdb },
    { 0x370a, 0x81 },
    { 0x370c, 0x00 },
    { 0x3713, 0x92 },
    { 0x3714, 0x17 },
    { 0x3010, 0x30 }, /* PLL1: 48MHz */
    { 0x3011, 0x10 }, /* PLL2 */
    { 0x3012, 0x02 }, /* PLL3 */
    { 0x3800, 0x02 }, { 0x3801, 0x54 }, /* HREF start: 596 */
    { 0x3802, 0x00 }, { 0x3803, 0xf8 }, /* VREF start: 248 */
    { 0x3804, 0x05 }, { 0x3805, 0x00 }, /* HREF width: 1280 */
    { 0x3806, 0x02 }, { 0x3807, 0xd0 }, /* VREF width: 720 */
    { 0x3808, 0x05 }, { 0x3809, 0x00 }, /* DVP H output: 1280 */
    { 0x380a, 0x02 }, { 0x380b, 0xd0 }, /* DVP V output: 720 */
    { 0x380c, 0x08 }, { 0x380d, 0x72 }, /* HTS: 2162 */
    { 0x380e, 0x02 }, { 0x380f, 0xe4 }, /* VTS: 740 */
    { 0x3815, 0x81 }, /* PCLK2SCLK ratio */
    { 0x3818, 0xc1 }, /* Mirror / flip */
    { 0x3819, 0x80 },
    { 0x381a, 0x00 },
    { 0x381c, 0x10 },
    { 0x381d, 0x82 },
    { 0x381e, 0x05 },
    { 0x381f, 0xc0 },
    { 0x3820, 0x00 },
    { 0x3821, 0x20 },
    { 0x3824, 0x23 },
    { 0x3825, 0x2c },
    { 0x3827, 0x0c },
    { 0x3503, 0x13 },
    { 0x350c, 0x00 },
    { 0x350d, 0x00 },
    { 0x3a01, 0x00 },
    { 0x3a00, 0x38 },
    { 0x3a13, 0x54 },
    { 0x401d, 0x28 },
    { 0x401c, 0x42 },
    { 0x5002, 0x00 },
    { 0x5900, 0x00 },
    { 0x5901, 0x00 },
    { 0x3621, 0xaf },
    { 0x3008, 0x02 },
    { 0x0000, 0x00 }
};

/* Mode 1: 1280x720 @ 60fps (High Speed 96MHz PCLK) */
static const struct ov5650_reg_val ov5650_regs_720p_60fps[] = {
    { 0x3613, 0xc4 },
    { 0x370d, 0x42 },
    { 0x3703, 0x9a },
    { 0x3705, 0xdb },
    { 0x370a, 0x81 },
    { 0x3010, 0x10 }, /* PLL1: 96MHz */
    { 0x3011, 0x10 },
    { 0x3012, 0x01 },
    { 0x3800, 0x02 }, { 0x3801, 0x54 },
    { 0x3802, 0x00 }, { 0x3803, 0xf8 },
    { 0x3804, 0x05 }, { 0x3805, 0x00 },
    { 0x3806, 0x02 }, { 0x3807, 0xd0 },
    { 0x3808, 0x05 }, { 0x3809, 0x00 },
    { 0x380a, 0x02 }, { 0x380b, 0xd0 },
    { 0x380c, 0x08 }, { 0x380d, 0x72 },
    { 0x380e, 0x02 }, { 0x380f, 0xe4 },
    { 0x3815, 0x81 },
    { 0x3820, 0x00 },
    { 0x3821, 0x20 },
    { 0x3503, 0x13 },
    { 0x3008, 0x02 },
    { 0x0000, 0x00 }
};

/* Mode 2: 1920x1080 @ 30fps (Full HD 96MHz PCLK) */
static const struct ov5650_reg_val ov5650_regs_1080p_30fps[] = {
    { 0x3613, 0x44 },
    { 0x370d, 0x04 },
    { 0x3703, 0xe6 },
    { 0x3705, 0xda },
    { 0x370a, 0x80 },
    { 0x370c, 0x00 },
    { 0x3713, 0x22 },
    { 0x3714, 0x27 },
    { 0x3010, 0x10 }, /* PLL1: 96MHz */
    { 0x3011, 0x10 },
    { 0x3012, 0x02 },
    { 0x3800, 0x02 }, { 0x3801, 0x94 }, /* HREF start: 660 */
    { 0x3802, 0x00 }, { 0x3803, 0x0a }, /* VREF start: 10 */
    { 0x3804, 0x07 }, { 0x3805, 0x80 }, /* HREF width: 1920 */
    { 0x3806, 0x04 }, { 0x3807, 0x38 }, /* VREF width: 1080 */
    { 0x3808, 0x07 }, { 0x3809, 0x80 }, /* DVP H output: 1920 */
    { 0x380a, 0x04 }, { 0x380b, 0x38 }, /* DVP V output: 1080 */
    { 0x380c, 0x0a }, { 0x380d, 0x84 }, /* HTS: 2692 */
    { 0x380e, 0x04 }, { 0x380f, 0xa4 }, /* VTS: 1188 */
    { 0x3815, 0x82 },
    { 0x3818, 0xc0 },
    { 0x3819, 0x80 },
    { 0x381a, 0x1c },
    { 0x381c, 0x31 },
    { 0x381d, 0xa4 },
    { 0x381e, 0x04 },
    { 0x381f, 0x60 },
    { 0x3820, 0x03 },
    { 0x3821, 0x1a },
    { 0x3824, 0x01 },
    { 0x3825, 0xb4 },
    { 0x3827, 0x0a },
    { 0x3503, 0x13 },
    { 0x350c, 0x00 },
    { 0x350d, 0x00 },
    { 0x3a01, 0x00 },
    { 0x3a00, 0x38 },
    { 0x3a13, 0x54 },
    { 0x401d, 0x28 },
    { 0x401c, 0x46 },
    { 0x5002, 0x00 },
    { 0x5900, 0x00 },
    { 0x5901, 0x00 },
    { 0x3621, 0x2f },
    { 0x3008, 0x02 },
    { 0x0000, 0x00 }
};

/* Mode 3: 640x480 @ 60fps (VGA 2x2 binned) */
static const struct ov5650_reg_val ov5650_regs_vga_60fps[] = {
    { 0x3613, 0xc4 },
    { 0x370d, 0x42 },
    { 0x3703, 0x9a },
    { 0x3705, 0xdb },
    { 0x3010, 0x30 },
    { 0x3011, 0x10 },
    { 0x3012, 0x02 },
    { 0x3800, 0x02 }, { 0x3801, 0x54 },
    { 0x3802, 0x00 }, { 0x3803, 0xf8 },
    { 0x3804, 0x02 }, { 0x3805, 0x80 }, /* HREF width: 640 */
    { 0x3806, 0x01 }, { 0x3807, 0xe0 }, /* VREF width: 480 */
    { 0x3808, 0x02 }, { 0x3809, 0x80 }, /* DVP H output: 640 */
    { 0x380a, 0x01 }, { 0x380b, 0xe0 }, /* DVP V output: 480 */
    { 0x380c, 0x08 }, { 0x380d, 0x72 },
    { 0x380e, 0x02 }, { 0x380f, 0xe4 },
    { 0x3815, 0x81 },
    { 0x3820, 0x41 }, /* 2x2 binning */
    { 0x3821, 0x07 },
    { 0x3503, 0x13 },
    { 0x3008, 0x02 },
    { 0x0000, 0x00 }
};

/* Mode 4: 2592x1944 @ 15fps (Native 5MP Full Frame QSXGA) */
static const struct ov5650_reg_val ov5650_regs_5mp_15fps[] = {
    { 0x3613, 0x44 },
    { 0x370d, 0x04 },
    { 0x3703, 0xe6 },
    { 0x3705, 0xda },
    { 0x370a, 0x80 },
    { 0x370c, 0x00 },
    { 0x3713, 0x22 },
    { 0x3714, 0x27 },
    { 0x3010, 0x30 }, /* PLL1: 48MHz */
    { 0x3011, 0x10 },
    { 0x3012, 0x02 },
    { 0x3800, 0x02 }, { 0x3801, 0x54 },
    { 0x3802, 0x00 }, { 0x3803, 0x0c },
    { 0x3804, 0x0a }, { 0x3805, 0x20 }, /* HREF width: 2592 */
    { 0x3806, 0x07 }, { 0x3807, 0x98 }, /* VREF width: 1944 */
    { 0x3808, 0x0a }, { 0x3809, 0x20 }, /* DVP H output: 2592 */
    { 0x380a, 0x07 }, { 0x380b, 0x98 }, /* DVP V output: 1944 */
    { 0x380c, 0x0c }, { 0x380d, 0xb4 }, /* HTS: 3252 */
    { 0x380e, 0x07 }, { 0x380f, 0xb0 }, /* VTS: 1968 */
    { 0x3815, 0x82 },
    { 0x3818, 0xc0 },
    { 0x3819, 0x80 },
    { 0x381a, 0x4a },
    { 0x381c, 0x20 },
    { 0x381d, 0x0a },
    { 0x381e, 0x01 },
    { 0x381f, 0x20 },
    { 0x3820, 0x00 }, /* Full array active */
    { 0x3821, 0x00 },
    { 0x3824, 0x01 },
    { 0x3825, 0xb4 },
    { 0x3827, 0x0a },
    { 0x3503, 0x03 },
    { 0x3008, 0x02 },
    { 0x0000, 0x00 }
};

static const struct ov5650_mode_desc g_ov5650_modes[OV5650_MODE_COUNT] = {
    [OV5650_MODE_720P_30FPS]  = { .width = 1280, .height = 720,  .fps = 30, .name = "720p@30fps",  .regs = ov5650_regs_720p_30fps },
    [OV5650_MODE_720P_60FPS]  = { .width = 1280, .height = 720,  .fps = 60, .name = "720p@60fps",  .regs = ov5650_regs_720p_60fps },
    [OV5650_MODE_1080P_30FPS] = { .width = 1920, .height = 1080, .fps = 30, .name = "1080p@30fps", .regs = ov5650_regs_1080p_30fps },
    [OV5650_MODE_VGA_60FPS]   = { .width = 640,  .height = 480,  .fps = 60, .name = "VGA@60fps",    .regs = ov5650_regs_vga_60fps },
    [OV5650_MODE_5MP_15FPS]   = { .width = 2592, .height = 1944, .fps = 15, .name = "5MP@15fps",    .regs = ov5650_regs_5mp_15fps },
};

static uint32_t g_current_mode = OV5650_MODE_720P_30FPS;
static uint32_t g_sensor_powered = 0;

/* Stub I2C write function to BCM1570 I2C controller */
static int bcm1570_i2c_write(uint8_t chip_addr, uint16_t reg, uint8_t val)
{
    (void)chip_addr;
    (void)reg;
    (void)val;
    return 0;
}

static void ov5650_write_array(const struct ov5650_reg_val *table)
{
    int i = 0;
    while (table[i].reg != 0x0000) {
        bcm1570_i2c_write(OV5650_I2C_ADDR, table[i].reg, table[i].val);
        i++;
    }
}

int sensor_ov5650_set_mode(enum ov5650_mode_id mode)
{
    if (mode >= OV5650_MODE_COUNT)
        return -1;

    /* Write mode timing registers */
    ov5650_write_array(g_ov5650_modes[mode].regs);
    g_current_mode = mode;

    /* Synchronize ISP scaler with physical sensor active array */
    isp_scaler_set_sensor_resolution(g_ov5650_modes[mode].width, g_ov5650_modes[mode].height);

    return 0;
}

int sensor_ov5650_find_mode(uint32_t width, uint32_t height)
{
    for (int i = 0; i < OV5650_MODE_COUNT; i++) {
        if (g_ov5650_modes[i].width == width && g_ov5650_modes[i].height == height)
            return i;
    }
    return OV5650_MODE_720P_30FPS;
}

const struct ov5650_mode_desc *sensor_ov5650_get_mode_desc(enum ov5650_mode_id mode)
{
    if (mode >= OV5650_MODE_COUNT)
        return &g_ov5650_modes[0];
    return &g_ov5650_modes[mode];
}

int sensor_ov5650_set_exposure(uint32_t lines)
{
    /* OV5650 exposure is a 20-bit value written to 0x3500-0x3502 */
    if (lines > 0xfffff)
        lines = 0xfffff;

    uint8_t exp_high = (uint8_t)((lines >> 16) & 0x0f);
    uint8_t exp_mid  = (uint8_t)((lines >> 8)  & 0xff);
    uint8_t exp_low  = (uint8_t)(lines & 0xff);

    bcm1570_i2c_write(OV5650_I2C_ADDR, OV5650_REG_AEC_EXPO_HIGH, exp_high);
    bcm1570_i2c_write(OV5650_I2C_ADDR, OV5650_REG_AEC_EXPO_MID,  exp_mid);
    bcm1570_i2c_write(OV5650_I2C_ADDR, OV5650_REG_AEC_EXPO_LOW,  exp_low);
    return 0;
}

int sensor_ov5650_set_gain(uint16_t gain_val)
{
    /* OV5650 gain register: 0x3503 (bits [6:0]) */
    uint8_t reg_val = (uint8_t)(gain_val & 0x7f);
    bcm1570_i2c_write(OV5650_I2C_ADDR, OV5650_REG_AGC_MANUAL, reg_val);
    return 0;
}

int sensor_ov5650_set_framerate(uint32_t fps)
{
    /* Adjust clock divider according to target fps */
    uint8_t clk_div = 0;
    switch (fps) {
    case 60: clk_div = 2; break;
    case 30: clk_div = 4; break;
    case 15: clk_div = 8; break;
    default: clk_div = 4; break;
    }
    bcm1570_i2c_write(OV5650_I2C_ADDR, OV5650_REG_PLL_CTRL3, clk_div);
    return 0;
}

int sensor_ov5650_power_on(void)
{
    g_sensor_powered = 1;
    ov5650_write_array(sensor_default_regs);
    return sensor_ov5650_set_mode(OV5650_MODE_720P_30FPS);
}

int sensor_ov5650_power_off(void)
{
    g_sensor_powered = 0;
    bcm1570_i2c_write(OV5650_I2C_ADDR, OV5650_REG_SYSTEM_RESET, 0x82);
    return 0;
}

int sensor_ov5650_init(void)
{
    return sensor_ov5650_power_on();
}
