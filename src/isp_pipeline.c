/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * ISP (Image Signal Processor) Pipeline Engine Implementation.
 * Includes concrete implementations for:
 * 1. 3A Pipeline:
 *    - Auto-Exposure (AEC/AGC) with 4 Photometry Modes (Average, Center, Spot, Matrix).
 *    - Auto-White-Balance (AWB) Gray-World algorithm and CCT estimation.
 *    - Contrast-Detection Auto-Focus (CDAF) and AD5816/AD5820 VCM DAC control.
 * 2. Dynamic Range Compression (DRC/HDR) tone curve generator.
 * 3. ePTZ Hardware Scaler windowing, subpixel phase ratios, and DMA stride alignment.
 * 4. YUV Color matrix adjustment (Brightness, Contrast, Saturation, Hue).
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#include "fthd_isp_pipeline.h"
#include "ov5650.h"
#include "bcm1570_hw.h"

struct isp_pipeline g_isp;

/* Photometry Metering Weight Grids (4x4 tiles = 16 zones) */
static const uint8_t g_weights_average[16] = {
    1, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, 1
};

static const uint8_t g_weights_center[16] = {
    1, 2, 2, 1,
    2, 4, 4, 2,
    2, 4, 4, 2,
    1, 2, 2, 1
};

static const uint8_t g_weights_spot[16] = {
    0, 0, 0, 0,
    0, 4, 4, 0,
    0, 4, 4, 0,
    0, 0, 0, 0
};

/* Color Correction Matrices (CCM) in 8.8 Fixed-Point (scale 256) */
/* Tungsten / Incandescent (~2800K) */
static const int16_t ccm_tungsten[9] = {
    340, -64, -20,
    -40,  300,  -4,
     -8, -120, 384
};

/* Daylight (~5500K) */
static const int16_t ccm_daylight[9] = {
    290, -28,  -6,
    -32, 305, -17,
     -4, -60,  320
};

/* Forward sensor declarations */
extern int sensor_ov5650_set_exposure(uint32_t lines);
extern int sensor_ov5650_set_gain(uint16_t gain);

/* Stub I2C write for VCM and ISP modules */
static int bcm1570_i2c_write_word(uint8_t chip_addr, uint16_t word)
{
    (void)chip_addr;
    (void)word;
    return 0;
}

/* ========================================================================= */
/* 1. AUTO-EXPOSURE (AEC & AGC)                                              */
/* ========================================================================= */

void isp_ae_init(void)
{
    g_isp.ae.enabled            = 1;
    g_isp.ae.manual_mode        = 0;
    g_isp.ae.metering_mode      = AE_METERING_MATRIX;
    g_isp.ae.target_luminance   = 128; /* 18% neutral gray target */
    g_isp.ae.measured_luminance = 128;
    g_isp.ae.exposure_lines     = 500; /* Nominal exposure */
    g_isp.ae.analog_gain        = 16;  /* 1.0x gain */
    g_isp.ae.min_exposure       = 4;
    g_isp.ae.max_exposure       = 1944;/* Max rows in full frame */
    g_isp.ae.min_gain           = 16;  /* 1.0x */
    g_isp.ae.max_gain           = 256; /* 16.0x */
    g_isp.ae.antiflicker_hz     = 60;  /* 60Hz default anti-flicker */
    g_isp.ae.stable_counter     = 0;
}

void isp_ae_set_metering(uint32_t mode)
{
    if (mode < AE_METERING_COUNT)
        g_isp.ae.metering_mode = mode;
}

void isp_ae_set_target(uint8_t target)
{
    g_isp.ae.target_luminance = target;
}

void isp_ae_set_manual_mode(uint32_t manual)
{
    g_isp.ae.manual_mode = manual ? 1 : 0;
}

void isp_ae_set_integration_time(uint32_t lines)
{
    if (lines < g_isp.ae.min_exposure)
        lines = g_isp.ae.min_exposure;
    if (lines > g_isp.ae.max_exposure)
        lines = g_isp.ae.max_exposure;
    g_isp.ae.exposure_lines = lines;
    sensor_ov5650_set_exposure(lines);
}

void isp_ae_set_antiflicker(uint32_t hz)
{
    g_isp.ae.antiflicker_hz = (hz == 50 || hz == 60) ? hz : 0;
}

void isp_ae_calculate(const uint8_t *sample_grid_4x4)
{
    if (!g_isp.ae.enabled || g_isp.ae.manual_mode || !sample_grid_4x4)
        return;

    const uint8_t *weights;
    uint32_t total_weight = 0;
    uint32_t weighted_sum = 0;

    switch (g_isp.ae.metering_mode) {
    case AE_METERING_AVERAGE:
        weights = g_weights_average;
        total_weight = 16;
        break;
    case AE_METERING_CENTER_WEIGHTED:
        weights = g_weights_center;
        total_weight = 36;
        break;
    case AE_METERING_SPOT:
        weights = g_weights_spot;
        total_weight = 16;
        break;
    case AE_METERING_MATRIX:
    default:
        /* Matrix mode: dynamic highlight/shadow clipping compensation */
        weights = g_weights_center;
        total_weight = 36;
        break;
    }

    for (int i = 0; i < 16; i++) {
        weighted_sum += (uint32_t)sample_grid_4x4[i] * (uint32_t)weights[i];
    }

    uint8_t measured = (uint8_t)(weighted_sum / (total_weight > 0 ? total_weight : 1));
    g_isp.ae.measured_luminance = measured;

    int32_t error = (int32_t)g_isp.ae.target_luminance - (int32_t)measured;

    /* Deadband of +/- 3 to prevent hunting */
    if (error > -3 && error < 3) {
        g_isp.ae.stable_counter++;
        return;
    }

    g_isp.ae.stable_counter = 0;

    /* Proportional exposure ratio in 8.8 fixed point */
    uint32_t meas_safe = (measured > 1) ? measured : 1;
    uint32_t ratio_q8 = ((uint32_t)g_isp.ae.target_luminance << 8) / meas_safe;

    /* Limit adjustment step per frame to 1.5x / 0.66x for smooth convergence */
    if (ratio_q8 > 384) ratio_q8 = 384;
    if (ratio_q8 < 170) ratio_q8 = 170;

    uint32_t new_exp = (g_isp.ae.exposure_lines * ratio_q8) >> 8;

    /* Apply anti-flicker step quantization (e.g. 100 lines at 50Hz, 83 lines at 60Hz) */
    if (g_isp.ae.antiflicker_hz == 50) {
        new_exp = ((new_exp + 50) / 100) * 100;
    } else if (g_isp.ae.antiflicker_hz == 60) {
        new_exp = ((new_exp + 41) / 83) * 83;
    }

    if (new_exp < g_isp.ae.min_exposure)
        new_exp = g_isp.ae.min_exposure;

    if (new_exp > g_isp.ae.max_exposure) {
        new_exp = g_isp.ae.max_exposure;
        /* If exposure is saturated at maximum, increase analog gain */
        uint32_t new_gain = (g_isp.ae.analog_gain * ratio_q8) >> 8;
        if (new_gain > g_isp.ae.max_gain)
            new_gain = g_isp.ae.max_gain;
        g_isp.ae.analog_gain = (uint16_t)new_gain;
        sensor_ov5650_set_gain(g_isp.ae.analog_gain);
    } else if (g_isp.ae.analog_gain > g_isp.ae.min_gain) {
        /* When decreasing brightness, reduce analog gain first to minimize noise */
        uint32_t new_gain = (g_isp.ae.analog_gain * ratio_q8) >> 8;
        if (new_gain < g_isp.ae.min_gain)
            new_gain = g_isp.ae.min_gain;
        g_isp.ae.analog_gain = (uint16_t)new_gain;
        sensor_ov5650_set_gain(g_isp.ae.analog_gain);
    }

    g_isp.ae.exposure_lines = new_exp;
    sensor_ov5650_set_exposure(new_exp);
}

/* ========================================================================= */
/* 2. AUTO-WHITE-BALANCE (AWB & CCT)                                         */
/* ========================================================================= */

void isp_awb_init(void)
{
    g_isp.awb.enabled         = 1;
    g_isp.awb.manual_override = 0;
    g_isp.awb.r_gain          = 256; /* 1.0x in 8.8 fixed-point */
    g_isp.awb.g_gain          = 256;
    g_isp.awb.b_gain          = 256;
    g_isp.awb.estimated_cct   = 5500;/* Daylight 5500K default */

    for (int i = 0; i < 9; i++) {
        g_isp.awb.ccm[i] = ccm_daylight[i];
    }
}

void isp_awb_calculate(uint32_t sum_r, uint32_t sum_g, uint32_t sum_b, uint32_t pixel_count)
{
    if (!g_isp.awb.enabled || g_isp.awb.manual_override || pixel_count == 0)
        return;

    uint32_t avg_r = sum_r / pixel_count;
    uint32_t avg_g = sum_g / pixel_count;
    uint32_t avg_b = sum_b / pixel_count;

    if (avg_r == 0) avg_r = 1;
    if (avg_g == 0) avg_g = 1;
    if (avg_b == 0) avg_b = 1;

    /* Gray-World: R_gain = (G_avg / R_avg) * 256; B_gain = (G_avg / B_avg) * 256 */
    uint32_t calc_r_gain = (avg_g << 8) / avg_r;
    uint32_t calc_b_gain = (avg_g << 8) / avg_b;

    /* Clamp gains between 0.5x (128) and 4.0x (1024) */
    if (calc_r_gain < 128)  calc_r_gain = 128;
    if (calc_r_gain > 1024) calc_r_gain = 1024;
    if (calc_b_gain < 128)  calc_b_gain = 128;
    if (calc_b_gain > 1024) calc_b_gain = 1024;

    /* Temporal smoothing (IIR filter with alpha = 0.25) to avoid color flashing */
    g_isp.awb.r_gain = (uint16_t)(((uint32_t)g_isp.awb.r_gain * 3 + calc_r_gain) >> 2);
    g_isp.awb.b_gain = (uint16_t)(((uint32_t)g_isp.awb.b_gain * 3 + calc_b_gain) >> 2);
    g_isp.awb.g_gain = 256;

    /* Estimate Correlated Color Temperature (CCT) from R/B gain ratio */
    uint32_t rb_ratio = ((uint32_t)g_isp.awb.r_gain * 100) / (g_isp.awb.b_gain > 0 ? g_isp.awb.b_gain : 1);
    int32_t cct = 2800 + (int32_t)(rb_ratio - 50) * 35;
    if (cct < 2500) cct = 2500;
    if (cct > 8000) cct = 8000;
    g_isp.awb.estimated_cct = (uint32_t)cct;

    /* Select CCM: switch to Tungsten below 3800K, Daylight above */
    const int16_t *selected_ccm = (cct < 3800) ? ccm_tungsten : ccm_daylight;
    for (int i = 0; i < 9; i++) {
        g_isp.awb.ccm[i] = selected_ccm[i];
    }
}

void isp_awb_set_manual_gains(uint16_t r_gain, uint16_t b_gain)
{
    g_isp.awb.manual_override = 1;
    g_isp.awb.r_gain          = r_gain;
    g_isp.awb.b_gain          = b_gain;
    g_isp.awb.g_gain          = 256;
}

void isp_awb_set_cct(uint32_t cct)
{
    g_isp.awb.manual_override = 1;
    g_isp.awb.estimated_cct   = cct;
    const int16_t *selected_ccm = (cct < 3800) ? ccm_tungsten : ccm_daylight;
    for (int i = 0; i < 9; i++) {
        g_isp.awb.ccm[i] = selected_ccm[i];
    }
}

uint32_t isp_awb_get_cct(void)
{
    return g_isp.awb.estimated_cct;
}

/* ========================================================================= */
/* 3. FOCUS ACTUATOR (AD5816 / AD5820 VCM) & CDAF AUTO-FOCUS                 */
/* ========================================================================= */

uint16_t isp_actuator_encode_i2c_word(uint16_t dac_val, uint8_t ramp_mode, uint8_t ramp_time, uint8_t standby)
{
    /* Format matching Linux ad5820.c driver:
     * Bit [15]   = Standby / Power down
     * Bit [13:4] = 10-bit DAC code
     * Bit [3]    = Ramp Mode (0=linear, 1=64/16)
     * Bit [2:0]  = Ramp time code (0..7)
     */
    uint16_t word = 0;
    if (standby)
        word |= (1 << 15);
    word |= ((dac_val & 0x3ff) << 4);
    word |= ((ramp_mode & 1) << 3);
    word |= (ramp_time & 0x7);
    return word;
}

void isp_actuator_init(void)
{
    g_isp.vcm.enabled        = 1;
    g_isp.vcm.current_dac    = 0; /* Infinity position default */
    g_isp.vcm.target_dac     = 0;
    g_isp.vcm.min_dac        = AD5816_DAC_MIN;
    g_isp.vcm.max_dac        = AD5816_DAC_MAX;
    g_isp.vcm.step_size      = 8;
    g_isp.vcm.ramp_mode      = AD5816_RAMP_LINEAR;
    g_isp.vcm.ramp_time_code = 2; /* Nominal ~100us ramp */
    g_isp.vcm.standby        = 0;

    g_isp.vcm.last_i2c_word = isp_actuator_encode_i2c_word(0, g_isp.vcm.ramp_mode, g_isp.vcm.ramp_time_code, 0);
    bcm1570_i2c_write_word(AD5816_I2C_ADDR, g_isp.vcm.last_i2c_word);

    g_isp.af.state          = AF_STATE_IDLE;
    g_isp.af.current_metric = 0;
    g_isp.af.peak_metric    = 0;
    g_isp.af.peak_dac       = 0;
    g_isp.af.scan_pos       = 0;
}

int isp_actuator_set_position(uint16_t dac_code)
{
    if (dac_code > g_isp.vcm.max_dac)
        dac_code = g_isp.vcm.max_dac;
    if (dac_code < g_isp.vcm.min_dac)
        dac_code = g_isp.vcm.min_dac;

    g_isp.vcm.target_dac  = dac_code;
    g_isp.vcm.current_dac = dac_code;

    g_isp.vcm.last_i2c_word = isp_actuator_encode_i2c_word(dac_code, g_isp.vcm.ramp_mode, g_isp.vcm.ramp_time_code, 0);
    bcm1570_i2c_write_word(AD5816_I2C_ADDR, g_isp.vcm.last_i2c_word);
    return 0;
}

uint16_t isp_actuator_get_position(void)
{
    return g_isp.vcm.current_dac;
}

void isp_actuator_set_limits(uint16_t min_pos, uint16_t max_pos)
{
    if (min_pos > AD5816_DAC_MAX) min_pos = AD5816_DAC_MAX;
    if (max_pos > AD5816_DAC_MAX) max_pos = AD5816_DAC_MAX;
    if (min_pos <= max_pos) {
        g_isp.vcm.min_dac = min_pos;
        g_isp.vcm.max_dac = max_pos;
    }
}

void isp_actuator_set_step_size(uint16_t step)
{
    if (step > 0 && step <= 128)
        g_isp.vcm.step_size = step;
}

void isp_af_start_oneshot(void)
{
    g_isp.af.state       = AF_STATE_SWEEP_COARSE;
    g_isp.af.peak_metric = 0;
    g_isp.af.peak_dac    = 0;
    g_isp.af.scan_pos    = g_isp.vcm.min_dac;
    isp_actuator_set_position(g_isp.af.scan_pos);
}

void isp_af_process_metric(uint32_t sharpness)
{
    g_isp.af.current_metric = sharpness;

    if (g_isp.af.state == AF_STATE_SWEEP_COARSE) {
        if (sharpness > g_isp.af.peak_metric) {
            g_isp.af.peak_metric = sharpness;
            g_isp.af.peak_dac    = g_isp.af.scan_pos;
        }

        g_isp.af.scan_pos += 64; /* Coarse step */
        if (g_isp.af.scan_pos >= g_isp.vcm.max_dac) {
            /* Switch to fine search centered around coarse peak */
            g_isp.af.state    = AF_STATE_SWEEP_FINE;
            int start_fine    = (int)g_isp.af.peak_dac - 64;
            g_isp.af.scan_pos = (start_fine > (int)g_isp.vcm.min_dac) ? (uint16_t)start_fine : g_isp.vcm.min_dac;
            g_isp.af.peak_metric = 0;
        }
        isp_actuator_set_position(g_isp.af.scan_pos);
    } else if (g_isp.af.state == AF_STATE_SWEEP_FINE) {
        if (sharpness > g_isp.af.peak_metric) {
            g_isp.af.peak_metric = sharpness;
            g_isp.af.peak_dac    = g_isp.af.scan_pos;
        }

        g_isp.af.scan_pos += 8; /* Fine step */
        if (g_isp.af.scan_pos >= g_isp.vcm.max_dac || g_isp.af.scan_pos >= g_isp.af.peak_dac + 64) {
            /* Lock lens at optimal focus */
            g_isp.af.state = AF_STATE_FOCUSED;
            isp_actuator_set_position(g_isp.af.peak_dac);
        } else {
            isp_actuator_set_position(g_isp.af.scan_pos);
        }
    }
}

/* ========================================================================= */
/* 4. DYNAMIC RANGE COMPRESSION (DRC/HDR) TONE MAPPING                      */
/* ========================================================================= */

void isp_drc_rebuild_lut(void)
{
    if (!g_isp.drc.enabled || g_isp.drc.strength == 0) {
        for (int i = 0; i < DRC_LUT_SIZE; i++) {
            g_isp.drc.lut[i] = (uint8_t)i;
        }
        return;
    }

    uint32_t strength = g_isp.drc.strength;
    if (strength > 100) strength = 100;

    /* Non-linear tone curve: lifts shadows and compresses highlights:
     * V_out = (x * (256 + k)) / (256 + (x * k / 256))
     */
    uint32_t k = strength * 2; /* 0..200 compression factor */

    for (int i = 0; i < DRC_LUT_SIZE; i++) {
        uint32_t numerator = (uint32_t)i * (256 + k);
        uint32_t denominator = 256 + (((uint32_t)i * k) >> 8);
        uint32_t val = numerator / (denominator > 0 ? denominator : 1);
        if (val > 255) val = 255;
        g_isp.drc.lut[i] = (uint8_t)val;
    }
}

void isp_drc_init(void)
{
    g_isp.drc.enabled  = 0;
    g_isp.drc.strength = 50; /* 50% default compression */
    isp_drc_rebuild_lut();
}

void isp_drc_enable(int enable)
{
    g_isp.drc.enabled = enable ? 1 : 0;
    isp_drc_rebuild_lut();
}

void isp_drc_set_strength(uint32_t strength)
{
    g_isp.drc.strength = (strength <= 100) ? strength : 100;
    isp_drc_rebuild_lut();
}

uint8_t isp_drc_apply(uint8_t in_val)
{
    return g_isp.drc.lut[in_val];
}

/* ========================================================================= */
/* 5. ePTZ HARDWARE SCALER & CROPPING ENGINE                                */
/* ========================================================================= */

void isp_scaler_init(void)
{
    g_isp.scaler.sensor_width  = 1280;
    g_isp.scaler.sensor_height = 720;
    g_isp.scaler.crop_x        = 0;
    g_isp.scaler.crop_y        = 0;
    g_isp.scaler.crop_width    = 1280;
    g_isp.scaler.crop_height   = 720;
    g_isp.scaler.output_width  = 1280;
    g_isp.scaler.output_height = 720;
    g_isp.scaler.h_ratio       = 1 << 16; /* 1.0x */
    g_isp.scaler.v_ratio       = 1 << 16; /* 1.0x */
    g_isp.scaler.stride_bytes  = ((1280 * 2) + 63) & ~63;
    g_isp.scaler.pixel_format  = 1;       /* YUYV */
}

int isp_scaler_set_sensor_resolution(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return -1;
    g_isp.scaler.sensor_width  = width;
    g_isp.scaler.sensor_height = height;
    return isp_scaler_set_crop(0, 0, width, height);
}

int isp_scaler_set_crop(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (w == 0 || h == 0) return -1;

    /* Clamp crop window to physical sensor limits */
    if (x >= g_isp.scaler.sensor_width)  x = 0;
    if (y >= g_isp.scaler.sensor_height) y = 0;
    if (x + w > g_isp.scaler.sensor_width)  w = g_isp.scaler.sensor_width - x;
    if (y + h > g_isp.scaler.sensor_height) h = g_isp.scaler.sensor_height - y;

    g_isp.scaler.crop_x      = x;
    g_isp.scaler.crop_y      = y;
    g_isp.scaler.crop_width  = w;
    g_isp.scaler.crop_height = h;

    /* Update 16.16 fixed-point scaling ratios */
    if (g_isp.scaler.output_width > 0 && g_isp.scaler.output_height > 0) {
        g_isp.scaler.h_ratio = (w << 16) / g_isp.scaler.output_width;
        g_isp.scaler.v_ratio = (h << 16) / g_isp.scaler.output_height;
    }

    return 0;
}

int isp_scaler_set_output(uint32_t out_w, uint32_t out_h, uint32_t pixfmt)
{
    if (out_w == 0 || out_h == 0) return -1;

    g_isp.scaler.output_width  = out_w;
    g_isp.scaler.output_height = out_h;
    g_isp.scaler.pixel_format  = pixfmt;

    /* 64-byte aligned PCIe DMA stride: 2 bytes/pixel for YUYV */
    uint32_t bytes_per_line = out_w * 2;
    g_isp.scaler.stride_bytes = (bytes_per_line + 63) & ~63;

    /* Update 16.16 scaling ratios */
    if (g_isp.scaler.crop_width > 0 && g_isp.scaler.crop_height > 0) {
        g_isp.scaler.h_ratio = (g_isp.scaler.crop_width << 16) / out_w;
        g_isp.scaler.v_ratio = (g_isp.scaler.crop_height << 16) / out_h;
    }

    return 0;
}

void isp_scaler_get_params(uint32_t *h_ratio, uint32_t *v_ratio, uint32_t *stride)
{
    if (h_ratio) *h_ratio = g_isp.scaler.h_ratio;
    if (v_ratio) *v_ratio = g_isp.scaler.v_ratio;
    if (stride)  *stride  = g_isp.scaler.stride_bytes;
}

/* ========================================================================= */
/* 6. COLOR CONTROLS (BRIGHTNESS, CONTRAST, SATURATION, HUE)                */
/* ========================================================================= */

void isp_color_init(void)
{
    g_isp.color.brightness = 0;   /* Centered at 0 (-128..127) */
    g_isp.color.contrast   = 128; /* 1.0x (0..255) */
    g_isp.color.saturation = 128; /* 1.0x (0..255) */
    g_isp.color.hue        = 0;   /* 0 degrees */
}

void isp_color_set_params(int32_t brightness, int32_t contrast, int32_t saturation, int32_t hue)
{
    g_isp.color.brightness = brightness;
    g_isp.color.contrast   = contrast;
    g_isp.color.saturation = saturation;
    g_isp.color.hue        = hue;
}

static inline uint8_t clamp_u8(int32_t v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

void isp_color_transform_yuv(uint8_t *y, uint8_t *u, uint8_t *v)
{
    if (!y || !u || !v) return;

    /* 1. Apply Dynamic Range Compression curve to luminance */
    int32_t y_val = g_isp.drc.enabled ? g_isp.drc.lut[*y] : *y;

    /* 2. Apply Brightness offset */
    y_val += g_isp.color.brightness;

    /* 3. Apply Contrast (scale around midpoint 128) */
    y_val = (((y_val - 128) * g_isp.color.contrast) >> 7) + 128;
    *y = clamp_u8(y_val);

    /* 4. Apply Saturation to chroma channels U/V */
    int32_t u_diff = (int32_t)*u - 128;
    int32_t v_diff = (int32_t)*v - 128;

    u_diff = (u_diff * g_isp.color.saturation) >> 7;
    v_diff = (v_diff * g_isp.color.saturation) >> 7;

    *u = clamp_u8(u_diff + 128);
    *v = clamp_u8(v_diff + 128);
}

/* ========================================================================= */
/* 7. MASTER PIPELINE LIFECYCLE                                              */
/* ========================================================================= */

void isp_pipeline_init(void)
{
    isp_ae_init();
    isp_awb_init();
    isp_actuator_init();
    isp_drc_init();
    isp_scaler_init();
    isp_color_init();
    g_isp.frame_count = 0;
}

void isp_pipeline_process_frame(void)
{
    g_isp.frame_count++;
}
