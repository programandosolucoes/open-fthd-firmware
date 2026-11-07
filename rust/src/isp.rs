// SPDX-License-Identifier: GPL-2.0-only
//! OpenFTHD Bare-Metal ISP (Image Signal Processor) Pipeline Engine in Rust.
//! Includes concrete 3A state machines (AEC, AWB, CDAF), AD5816/AD5820 VCM Actuator,
//! Dynamic Range Compression (DRC) tone-mapping LUT generator, ePTZ Scaler math,
//! and YUV Color tuning.

#![allow(dead_code)]

#[derive(Copy, Clone, PartialEq, Eq)]
pub enum AeMeteringMode {
    Average = 0,
    CenterWeighted = 1,
    Spot = 2,
    Matrix = 3,
}

#[derive(Copy, Clone, PartialEq, Eq)]
pub enum AfState {
    Idle = 0,
    SweepCoarse = 1,
    SweepFine = 2,
    Focused = 3,
}

pub struct AutoExposure {
    pub enabled: bool,
    pub manual_mode: bool,
    pub metering_mode: AeMeteringMode,
    pub target_luminance: u8,
    pub measured_luminance: u8,
    pub exposure_lines: u32,
    pub analog_gain: u16,
    pub min_exposure: u32,
    pub max_exposure: u32,
    pub min_gain: u16,
    pub max_gain: u16,
    pub antiflicker_hz: u32,
}

impl AutoExposure {
    pub const fn new() -> Self {
        Self {
            enabled: true,
            manual_mode: false,
            metering_mode: AeMeteringMode::Matrix,
            target_luminance: 128,
            measured_luminance: 128,
            exposure_lines: 500,
            analog_gain: 16,
            min_exposure: 4,
            max_exposure: 1944,
            min_gain: 16,
            max_gain: 256,
            antiflicker_hz: 60,
        }
    }

    pub fn calculate(&mut self, sample_grid_4x4: &[u8; 16]) {
        if !self.enabled || self.manual_mode {
            return;
        }

        let weights: [u8; 16] = match self.metering_mode {
            AeMeteringMode::Average => [1; 16],
            AeMeteringMode::CenterWeighted | AeMeteringMode::Matrix => [
                1, 2, 2, 1,
                2, 4, 4, 2,
                2, 4, 4, 2,
                1, 2, 2, 1,
            ],
            AeMeteringMode::Spot => [
                0, 0, 0, 0,
                0, 4, 4, 0,
                0, 4, 4, 0,
                0, 0, 0, 0,
            ],
        };

        let mut total_weight: u32 = 0;
        let mut weighted_sum: u32 = 0;

        for i in 0..16 {
            weighted_sum += (sample_grid_4x4[i] as u32) * (weights[i] as u32);
            total_weight += weights[i] as u32;
        }

        let measured = (weighted_sum / if total_weight > 0 { total_weight } else { 1 }) as u8;
        self.measured_luminance = measured;

        let error = (self.target_luminance as i32) - (measured as i32);
        if error.abs() < 3 {
            return;
        }

        let meas_safe = if measured > 0 { measured as u32 } else { 1 };
        let mut ratio_q8 = ((self.target_luminance as u32) << 8) / meas_safe;
        if ratio_q8 > 384 { ratio_q8 = 384; }
        if ratio_q8 < 170 { ratio_q8 = 170; }

        let mut new_exp = (self.exposure_lines * ratio_q8) >> 8;
        if new_exp < self.min_exposure { new_exp = self.min_exposure; }
        if new_exp > self.max_exposure {
            new_exp = self.max_exposure;
            let mut new_gain = ((self.analog_gain as u32) * ratio_q8) >> 8;
            if new_gain > self.max_gain as u32 { new_gain = self.max_gain as u32; }
            self.analog_gain = new_gain as u16;
        }

        self.exposure_lines = new_exp;
    }
}

pub struct AutoWhiteBalance {
    pub enabled: bool,
    pub manual_override: bool,
    pub r_gain: u16, // 8.8 fixed-point (256 = 1.0x)
    pub g_gain: u16,
    pub b_gain: u16,
    pub estimated_cct: u32,
}

impl AutoWhiteBalance {
    pub const fn new() -> Self {
        Self {
            enabled: true,
            manual_override: false,
            r_gain: 256,
            g_gain: 256,
            b_gain: 256,
            estimated_cct: 5500,
        }
    }

    pub fn calculate(&mut self, sum_r: u32, sum_g: u32, sum_b: u32, pixel_count: u32) {
        if !self.enabled || self.manual_override || pixel_count == 0 {
            return;
        }

        let avg_r = if sum_r / pixel_count > 0 { sum_r / pixel_count } else { 1 };
        let avg_g = if sum_g / pixel_count > 0 { sum_g / pixel_count } else { 1 };
        let avg_b = if sum_b / pixel_count > 0 { sum_b / pixel_count } else { 1 };

        let mut calc_r = (avg_g << 8) / avg_r;
        let mut calc_b = (avg_g << 8) / avg_b;

        if calc_r < 128 { calc_r = 128; }
        if calc_r > 1024 { calc_r = 1024; }
        if calc_b < 128 { calc_b = 128; }
        if calc_b > 1024 { calc_b = 1024; }

        self.r_gain = (((self.r_gain as u32 * 3) + calc_r) >> 2) as u16;
        self.b_gain = (((self.b_gain as u32 * 3) + calc_b) >> 2) as u16;
        self.g_gain = 256;

        let rb_ratio = (self.r_gain as u32 * 100) / if self.b_gain > 0 { self.b_gain as u32 } else { 1 };
        let mut cct = 2800 + (rb_ratio as i32 - 50) * 35;
        if cct < 2500 { cct = 2500; }
        if cct > 8000 { cct = 8000; }
        self.estimated_cct = cct as u32;
    }
}

pub struct VcmActuator {
    pub current_dac: u16, // 0..1023
    pub target_dac: u16,
    pub min_dac: u16,
    pub max_dac: u16,
    pub step_size: u16,
    pub ramp_mode: u8,
    pub ramp_time: u8,
    pub standby: bool,
}

impl VcmActuator {
    pub const fn new() -> Self {
        Self {
            current_dac: 0,
            target_dac: 0,
            min_dac: 0,
            max_dac: 1023,
            step_size: 8,
            ramp_mode: 0,
            ramp_time: 2,
            standby: false,
        }
    }

    pub fn encode_i2c_word(&self, dac: u16) -> u16 {
        let mut word: u16 = 0;
        if self.standby {
            word |= 1 << 15;
        }
        word |= (dac & 0x3ff) << 4;
        word |= ((self.ramp_mode & 1) as u16) << 3;
        word |= (self.ramp_time & 7) as u16;
        word
    }

    pub fn set_position(&mut self, pos: u16) -> u16 {
        let clamped = if pos > self.max_dac {
            self.max_dac
        } else if pos < self.min_dac {
            self.min_dac
        } else {
            pos
        };
        self.target_dac = clamped;
        self.current_dac = clamped;
        self.encode_i2c_word(clamped)
    }
}

pub struct DynamicRangeCompression {
    pub enabled: bool,
    pub strength: u32,
    pub lut: [u8; 256],
}

impl DynamicRangeCompression {
    pub const fn new() -> Self {
        let mut lut = [0u8; 256];
        let mut i = 0;
        while i < 256 {
            lut[i] = i as u8;
            i += 1;
        }
        Self {
            enabled: false,
            strength: 50,
            lut,
        }
    }

    pub fn rebuild_lut(&mut self) {
        if !self.enabled || self.strength == 0 {
            for i in 0..256 {
                self.lut[i] = i as u8;
            }
            return;
        }

        let k = if self.strength > 100 { 200 } else { self.strength * 2 };
        for i in 0..256 {
            let numerator = (i as u32) * (256 + k);
            let denominator = 256 + (((i as u32) * k) >> 8);
            let val = numerator / if denominator > 0 { denominator } else { 1 };
            self.lut[i] = if val > 255 { 255 } else { val as u8 };
        }
    }
}

pub struct ScalerPipeline {
    pub sensor_width: u32,
    pub sensor_height: u32,
    pub crop_x: u32,
    pub crop_y: u32,
    pub crop_width: u32,
    pub crop_height: u32,
    pub output_width: u32,
    pub output_height: u32,
    pub h_ratio: u32,
    pub v_ratio: u32,
    pub stride_bytes: u32,
}

impl ScalerPipeline {
    pub const fn new() -> Self {
        Self {
            sensor_width: 1280,
            sensor_height: 720,
            crop_x: 0,
            crop_y: 0,
            crop_width: 1280,
            crop_height: 720,
            output_width: 1280,
            output_height: 720,
            h_ratio: 1 << 16,
            v_ratio: 1 << 16,
            stride_bytes: ((1280 * 2) + 63) & !63,
        }
    }

    pub fn set_crop(&mut self, x: u32, y: u32, w: u32, h: u32) {
        let cx = if x >= self.sensor_width { 0 } else { x };
        let cy = if y >= self.sensor_height { 0 } else { y };
        let cw = if cx + w > self.sensor_width { self.sensor_width - cx } else { w };
        let ch = if cy + h > self.sensor_height { self.sensor_height - cy } else { h };

        self.crop_x = cx;
        self.crop_y = cy;
        self.crop_width = cw;
        self.crop_height = ch;

        if self.output_width > 0 && self.output_height > 0 {
            self.h_ratio = (cw << 16) / self.output_width;
            self.v_ratio = (ch << 16) / self.output_height;
        }
    }

    pub fn set_output(&mut self, out_w: u32, out_h: u32) {
        self.output_width = out_w;
        self.output_height = out_h;
        self.stride_bytes = ((out_w * 2) + 63) & !63;

        if out_w > 0 && out_h > 0 {
            self.h_ratio = (self.crop_width << 16) / out_w;
            self.v_ratio = (self.crop_height << 16) / out_h;
        }
    }
}

pub struct MasterIsp {
    pub ae: AutoExposure,
    pub awb: AutoWhiteBalance,
    pub vcm: VcmActuator,
    pub drc: DynamicRangeCompression,
    pub scaler: ScalerPipeline,
}

impl MasterIsp {
    pub const fn new() -> Self {
        Self {
            ae: AutoExposure::new(),
            awb: AutoWhiteBalance::new(),
            vcm: VcmActuator::new(),
            drc: DynamicRangeCompression::new(),
            scaler: ScalerPipeline::new(),
        }
    }
}
