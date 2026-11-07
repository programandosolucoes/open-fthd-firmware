/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * ISP (Image Signal Processor) Pipeline Engine Definitions.
 * Includes 3A (Auto-Exposure, Auto-White-Balance, Auto-Focus), Dynamic Range
 * Compression (DRC/HDR), AD5816/AD5820 Voice Coil Motor Actuator control,
 * and ePTZ Hardware Scaler Windowing.
 *
 * Derived cleanly from Linux kernel V4L2 subsystem specifications.
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#ifndef _OPENFTHD_ISP_PIPELINE_H
#define _OPENFTHD_ISP_PIPELINE_H

#include <stdint.h>
#include <stddef.h>

/* Photometry Metering Modes */
enum isp_ae_metering_mode {
    AE_METERING_AVERAGE         = 0, /* Uniform 1:1 weighting */
    AE_METERING_CENTER_WEIGHTED = 1, /* Center 4x4 priority */
    AE_METERING_SPOT            = 2, /* Central 2x2 spot only */
    AE_METERING_MATRIX          = 3, /* Multi-zone matrix with highlight/shadow compensation */
    AE_METERING_COUNT
};

/* Auto-Focus Search States (CDAF: Contrast Detection Auto-Focus) */
enum isp_af_state_id {
    AF_STATE_IDLE               = 0,
    AF_STATE_SWEEP_COARSE       = 1,
    AF_STATE_SWEEP_FINE         = 2,
    AF_STATE_FOCUSED            = 3,
};

/* AD5816 / AD5820 VCM Actuator Definitions */
#define AD5816_I2C_ADDR             0x0e
#define AD5816_DAC_MIN              0
#define AD5816_DAC_MAX              1023
#define AD5816_RAMP_LINEAR          0
#define AD5816_RAMP_64_16           1

/* Dynamic Range Compression Parameters */
#define DRC_LUT_SIZE                256

/* Auto-Exposure State Machine */
struct isp_ae_state {
    uint32_t enabled;
    uint32_t manual_mode;
    uint32_t metering_mode;     /* enum isp_ae_metering_mode */
    uint8_t  target_luminance;  /* Default 128 (18% gray) */
    uint8_t  measured_luminance;
    uint32_t exposure_lines;    /* Current integration lines */
    uint16_t analog_gain;       /* 16 = 1.0x, 256 = 16.0x */
    uint32_t min_exposure;
    uint32_t max_exposure;
    uint16_t min_gain;
    uint16_t max_gain;
    uint32_t antiflicker_hz;    /* 0=off, 50=50Hz, 60=60Hz */
    uint32_t stable_counter;
};

/* Auto-White-Balance State Machine */
struct isp_awb_state {
    uint32_t enabled;
    uint32_t manual_override;
    uint16_t r_gain;            /* 8.8 fixed-point (256 = 1.0x) */
    uint16_t g_gain;            /* 8.8 fixed-point (256 = 1.0x) */
    uint16_t b_gain;            /* 8.8 fixed-point (256 = 1.0x) */
    uint32_t estimated_cct;     /* Correlated Color Temperature in Kelvin */
    int16_t  ccm[9];            /* 3x3 Color Correction Matrix (scale 256) */
};

/* AD5816 / AD5820 Voice Coil Motor Focus Actuator State */
struct isp_actuator_state {
    uint32_t enabled;
    uint16_t current_dac;       /* 0 (Infinity) to 1023 (Macro) */
    uint16_t target_dac;
    uint16_t min_dac;
    uint16_t max_dac;
    uint16_t step_size;
    uint8_t  ramp_mode;
    uint8_t  ramp_time_code;
    uint8_t  standby;
    uint16_t last_i2c_word;
};

/* Contrast-Detection Auto-Focus Loop */
struct isp_af_state {
    uint32_t state;             /* enum isp_af_state_id */
    uint32_t current_metric;
    uint32_t peak_metric;
    uint16_t peak_dac;
    uint16_t scan_pos;
};

/* Dynamic Range Compression (DRC/HDR) State */
struct isp_drc_state {
    uint32_t enabled;
    uint32_t strength;          /* 0 = off, 100 = full curve */
    uint8_t  lut[DRC_LUT_SIZE];
};

/* ePTZ Hardware Scaler & Cropping State */
struct isp_scaler_state {
    uint32_t sensor_width;
    uint32_t sensor_height;
    uint32_t crop_x;
    uint32_t crop_y;
    uint32_t crop_width;
    uint32_t crop_height;
    uint32_t output_width;
    uint32_t output_height;
    uint32_t h_ratio;           /* 16.16 fixed-point scaling ratio */
    uint32_t v_ratio;           /* 16.16 fixed-point scaling ratio */
    uint32_t stride_bytes;      /* 64-byte aligned DMA line stride */
    uint32_t pixel_format;      /* 1 = YUYV */
};

/* Color Processing Matrix */
struct isp_color_tuning {
    int32_t brightness;         /* -128 to +127 (centered at 0, or 0..255) */
    int32_t contrast;           /* 0 to 255 (128 = 1.0x) */
    int32_t saturation;         /* 0 to 255 (128 = 1.0x) */
    int32_t hue;                /* 0 to 255 (representing -180 to +180 deg) */
};

/* Master ISP Pipeline Container */
struct isp_pipeline {
    struct isp_ae_state       ae;
    struct isp_awb_state      awb;
    struct isp_actuator_state vcm;
    struct isp_af_state       af;
    struct isp_drc_state      drc;
    struct isp_scaler_state   scaler;
    struct isp_color_tuning   color;
    uint32_t                  frame_count;
};

extern struct isp_pipeline g_isp;

/* ISP Pipeline Functions */
void isp_pipeline_init(void);
void isp_pipeline_process_frame(void);

/* 3A Auto-Exposure */
void isp_ae_init(void);
void isp_ae_set_metering(uint32_t mode);
void isp_ae_set_target(uint8_t target);
void isp_ae_set_manual_mode(uint32_t manual);
void isp_ae_set_integration_time(uint32_t lines);
void isp_ae_set_antiflicker(uint32_t hz);
void isp_ae_calculate(const uint8_t *sample_grid_4x4);

/* 3A Auto-White-Balance */
void isp_awb_init(void);
void isp_awb_calculate(uint32_t sum_r, uint32_t sum_g, uint32_t sum_b, uint32_t pixel_count);
void isp_awb_set_manual_gains(uint16_t r_gain, uint16_t b_gain);
void isp_awb_set_cct(uint32_t cct);
uint32_t isp_awb_get_cct(void);

/* 3A Auto-Focus & AD5816/AD5820 VCM Actuator */
void isp_actuator_init(void);
int  isp_actuator_set_position(uint16_t dac_code);
uint16_t isp_actuator_get_position(void);
void isp_actuator_set_limits(uint16_t min_pos, uint16_t max_pos);
void isp_actuator_set_step_size(uint16_t step);
uint16_t isp_actuator_encode_i2c_word(uint16_t dac_val, uint8_t ramp_mode, uint8_t ramp_time, uint8_t standby);
void isp_af_start_oneshot(void);
void isp_af_process_metric(uint32_t sharpness);

/* Dynamic Range Compression (DRC/HDR) */
void isp_drc_init(void);
void isp_drc_enable(int enable);
void isp_drc_set_strength(uint32_t strength);
void isp_drc_rebuild_lut(void);
uint8_t isp_drc_apply(uint8_t in_val);

/* ePTZ Hardware Scaler & Cropping */
void isp_scaler_init(void);
int  isp_scaler_set_sensor_resolution(uint32_t width, uint32_t height);
int  isp_scaler_set_crop(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
int  isp_scaler_set_output(uint32_t out_w, uint32_t out_h, uint32_t pixfmt);
void isp_scaler_get_params(uint32_t *h_ratio, uint32_t *v_ratio, uint32_t *stride);

/* Color Controls */
void isp_color_init(void);
void isp_color_set_params(int32_t brightness, int32_t contrast, int32_t saturation, int32_t hue);
void isp_color_transform_yuv(uint8_t *y, uint8_t *u, uint8_t *v);

#endif /* _OPENFTHD_ISP_PIPELINE_H */
