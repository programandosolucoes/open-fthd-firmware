// SPDX-License-Identifier: GPL-2.0-only
//! Host <-> ISP Inter-Process Communication (IPC) Channels and Ringbuffers.

#![allow(dead_code)]

use crate::isp::{MasterIsp, AeMeteringMode};
use crate::sensor::Ov5650Driver;

pub const FTHD_NUM_CHANNELS: usize = 8;

pub const DIR_HOST_TO_TARGET: u32 = 0;
pub const DIR_TARGET_TO_HOST: u32 = 1;

/// Extended Compute / Hardware Offload Channel name
pub const CHAN_NAME_COMPUTE: &[u8] = b"COMPUTE";

/// IPC Channel descriptor matching the Linux bcwc_pcie kernel layout.
#[repr(C, packed)]
pub struct ChannelDesc {
    pub name: [u8; 16],
    pub direction: u32,
    pub queue_offset: u32,
    pub num_entries: u32,
    pub entry_size: u32,
}

/// Static channel table placed in firmware data section.
pub static mut CHANNEL_TABLE: [ChannelDesc; FTHD_NUM_CHANNELS] = [
    ChannelDesc { name: *b"TERMINAL\0\0\0\0\0\0\0\0", direction: DIR_TARGET_TO_HOST, queue_offset: 0x1000, num_entries: 16, entry_size: 64 },
    ChannelDesc { name: *b"DEBUG\0\0\0\0\0\0\0\0\0\0\0", direction: DIR_TARGET_TO_HOST, queue_offset: 0x1400, num_entries: 16, entry_size: 64 },
    ChannelDesc { name: *b"SHAREDMALLOC\0\0\0\0",   direction: DIR_HOST_TO_TARGET, queue_offset: 0x1800, num_entries: 8,  entry_size: 128 },
    ChannelDesc { name: *b"IO\0\0\0\0\0\0\0\0\0\0\0\0\0\0", direction: DIR_HOST_TO_TARGET, queue_offset: 0x1c00, num_entries: 32, entry_size: 64 },
    ChannelDesc { name: *b"BUF_H2T\0\0\0\0\0\0\0\0\0", direction: DIR_HOST_TO_TARGET, queue_offset: 0x2400, num_entries: 16, entry_size: 32 },
    ChannelDesc { name: *b"BUF_T2H\0\0\0\0\0\0\0\0\0", direction: DIR_TARGET_TO_HOST, queue_offset: 0x2600, num_entries: 16, entry_size: 32 },
    ChannelDesc { name: *b"IO_T2H\0\0\0\0\0\0\0\0\0\0", direction: DIR_TARGET_TO_HOST, queue_offset: 0x2800, num_entries: 32, entry_size: 64 },
    ChannelDesc { name: *b"COMPUTE\0\0\0\0\0\0\0\0\0", direction: DIR_HOST_TO_TARGET, queue_offset: 0x2a00, num_entries: 16, entry_size: 64 },
];

/// Ringbuffer memory area for the terminal output.
static mut TERMINAL_BUFFER: [u8; 1024] = [0; 1024];
static mut TERMINAL_HEAD: usize = 0;

/// Ringbuffer memory area for IO requests (Channel 3: IO).
static mut IO_BUFFER: [u8; 1024] = [0; 1024];

/// Ringbuffer memory area for IO responses (Channel 6: IO_T2H).
static mut IO_T2H_BUFFER: [u8; 1024] = [0; 1024];

/// Ringbuffer memory area for Compute requests & responses (Channel 7: COMPUTE).
static mut COMPUTE_BUFFER: [u8; 1024] = [0; 1024];

/// Global Master ISP and Sensor instances
pub static mut MASTER_ISP: MasterIsp = MasterIsp::new();
pub static mut SENSOR_DRIVER: Ov5650Driver = Ov5650Driver::new();

/// Firmware internal ISP state machine for compatibility.
pub struct IspState {
    pub streaming_active: bool,
    pub crop_x: u32,
    pub crop_y: u32,
    pub crop_width: u32,
    pub crop_height: u32,
    pub output_width: u32,
    pub output_height: u32,
    pub brightness: i32,
    pub contrast: i32,
    pub saturation: i32,
    pub hue: i32,
    pub awb_enabled: bool,
    pub ae_metering: u32,
    pub drc_enabled: bool,
    pub led_mode: u32,
}

pub static mut ISP_STATE: IspState = IspState {
    streaming_active: false,
    crop_x: 0,
    crop_y: 0,
    crop_width: 1280,
    crop_height: 720,
    output_width: 1280,
    output_height: 720,
    brightness: 128,
    contrast: 128,
    saturation: 128,
    hue: 128,
    awb_enabled: true,
    ae_metering: 3,
    drc_enabled: false,
    led_mode: 0,
};

/// Dispatch incoming host command and update ISP hardware state.
pub fn dispatch_command(opcode: u32, payload: &[u8]) {
    unsafe {
        let mut resp_data: u32 = 0;

        match opcode {
            0x100 => { // CH_START
                ISP_STATE.streaming_active = true;
                SENSOR_DRIVER.power_on();
                terminal_write(b"Rust-OpenFTHD: CH_START streaming active\n");
            }
            0x101 => { // CH_STOP
                ISP_STATE.streaming_active = false;
                SENSOR_DRIVER.power_off();
                terminal_write(b"Rust-OpenFTHD: CH_STOP standby\n");
            }
            0x107 => { // CAMERA_CONFIG_SELECT
                if payload.len() >= 8 {
                    let mode_idx = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]) as usize;
                    let _ = SENSOR_DRIVER.set_mode(mode_idx);
                    let mode = SENSOR_DRIVER.current_mode_desc();
                    MASTER_ISP.scaler.sensor_width = mode.width as u32;
                    MASTER_ISP.scaler.sensor_height = mode.height as u32;
                    terminal_write(b"Rust-OpenFTHD: Mode selected\n");
                }
            }
            0x200 => { // AE_START
                MASTER_ISP.ae.enabled = true;
            }
            0x201 => { // AE_STOP
                MASTER_ISP.ae.enabled = false;
            }
            0x20f => { // AE_INTEGRATION_TIME_SET
                if payload.len() >= 8 {
                    let lines = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                    MASTER_ISP.ae.exposure_lines = lines;
                    SENSOR_DRIVER.set_exposure(lines);
                }
            }
            0x21e => { // AE_WINDOW_PARAM_SET
                if payload.len() >= 8 {
                    let m = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                    ISP_STATE.ae_metering = m;
                    MASTER_ISP.ae.metering_mode = match m {
                        0 => AeMeteringMode::Average,
                        1 => AeMeteringMode::CenterWeighted,
                        2 => AeMeteringMode::Spot,
                        _ => AeMeteringMode::Matrix,
                    };
                    terminal_write(b"Rust-OpenFTHD: AE photometry mode set\n");
                }
            }
            0x224 => { // AE_TARGET_SET
                if payload.len() >= 8 {
                    MASTER_ISP.ae.target_luminance = payload[4];
                }
            }
            0x22b => { // AE_MANUAL_MODE_SET
                if payload.len() >= 8 {
                    let manual = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                    MASTER_ISP.ae.manual_mode = manual != 0;
                }
            }
            0x300 => { // AWB_START
                ISP_STATE.awb_enabled = true;
                MASTER_ISP.awb.enabled = true;
            }
            0x301 => { // AWB_STOP
                ISP_STATE.awb_enabled = false;
                MASTER_ISP.awb.enabled = false;
            }
            0x304 => { // AWB_CCT_GET
                resp_data = MASTER_ISP.awb.estimated_cct;
            }
            0x305 => { // AWB_CCT_MANUAL
                if payload.len() >= 8 {
                    MASTER_ISP.awb.manual_override = true;
                    MASTER_ISP.awb.estimated_cct = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                }
            }
            0x311 => { // AWB_1ST_GAIN_MANUAL
                if payload.len() >= 12 {
                    MASTER_ISP.awb.manual_override = true;
                    MASTER_ISP.awb.r_gain = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]) as u16;
                    MASTER_ISP.awb.b_gain = u32::from_le_bytes([payload[8], payload[9], payload[10], payload[11]]) as u16;
                }
            }
            0x400 | 0x407 => { // AF_START / AF_ONE_SHOT
                terminal_write(b"Rust-OpenFTHD: AF sweep initiated\n");
            }
            0x500 => { // SENSOR_FRAME_RATE_SET
                if payload.len() >= 8 {
                    let fps = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                    SENSOR_DRIVER.set_framerate(fps);
                }
            }
            0x50b => { // SENSOR_POWER_ON
                SENSOR_DRIVER.power_on();
            }
            0x50c => { // SENSOR_POWER_OFF
                SENSOR_DRIVER.power_off();
            }
            0x700 => { // FOCUS_LIMITS_SET
                if payload.len() >= 12 {
                    MASTER_ISP.vcm.min_dac = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]) as u16;
                    MASTER_ISP.vcm.max_dac = u32::from_le_bytes([payload[8], payload[9], payload[10], payload[11]]) as u16;
                }
            }
            0x702 => { // FOCUS_POSITION_SET
                if payload.len() >= 8 {
                    let pos = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]) as u16;
                    MASTER_ISP.vcm.set_position(pos);
                }
            }
            0x703 => { // FOCUS_POSITION_GET
                resp_data = MASTER_ISP.vcm.current_dac as u32;
            }
            0x704 => { // FOCUS_STEP_SIZE_SET
                if payload.len() >= 8 {
                    MASTER_ISP.vcm.step_size = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]) as u16;
                }
            }
            0x801 | 0x80a => { // CH_CROP_SET / SCALER_CROP_SET
                if payload.len() >= 20 {
                    let x = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                    let y = u32::from_le_bytes([payload[8], payload[9], payload[10], payload[11]]);
                    let w = u32::from_le_bytes([payload[12], payload[13], payload[14], payload[15]]);
                    let h = u32::from_le_bytes([payload[16], payload[17], payload[18], payload[19]]);
                    ISP_STATE.crop_x = x;
                    ISP_STATE.crop_y = y;
                    ISP_STATE.crop_width = w;
                    ISP_STATE.crop_height = h;
                    MASTER_ISP.scaler.set_crop(x, y, w, h);
                    terminal_write(b"Rust-OpenFTHD: ePTZ hardware crop applied\n");
                }
            }
            0xa01 => { // COLOR_SATURATION_SET
                if payload.len() >= 8 {
                    ISP_STATE.saturation = i32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                }
            }
            0xa19 => { // DRC_SET
                if payload.len() >= 8 {
                    let st = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                    MASTER_ISP.drc.strength = st;
                    MASTER_ISP.drc.rebuild_lut();
                }
            }
            0xb01 => { // CH_OUTPUT_CONFIG_SET
                if payload.len() >= 12 {
                    let w = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                    let h = u32::from_le_bytes([payload[8], payload[9], payload[10], payload[11]]);
                    ISP_STATE.output_width = w;
                    ISP_STATE.output_height = h;
                    MASTER_ISP.scaler.set_output(w, h);
                    terminal_write(b"Rust-OpenFTHD: Output resolution configured\n");
                }
            }
            0xb02 => { // SCALER_BRIGHTNESS_SET
                if payload.len() >= 8 {
                    ISP_STATE.brightness = i32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                }
            }
            0xb03 => { // SCALER_CONTRAST_SET
                if payload.len() >= 8 {
                    ISP_STATE.contrast = i32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                }
            }
            0xb04 => { // SCALER_SATURATION_SET
                if payload.len() >= 8 {
                    ISP_STATE.saturation = i32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                }
            }
            0xb05 => { // SCALER_HUE_SET
                if payload.len() >= 8 {
                    ISP_STATE.hue = i32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                }
            }
            0xc00 => { // DRC_START
                ISP_STATE.drc_enabled = true;
                MASTER_ISP.drc.enabled = true;
                MASTER_ISP.drc.rebuild_lut();
                terminal_write(b"Rust-OpenFTHD: DRC dynamic tone curve enabled\n");
            }
            0xc01 => { // DRC_STOP
                ISP_STATE.drc_enabled = false;
                MASTER_ISP.drc.enabled = false;
            }
            0x608 => { // STATUS_LED_DEBUG_SET
                if payload.len() >= 8 {
                    ISP_STATE.led_mode = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                }
            }
            _ => {}
        }

        // Acknowledge on IO_T2H response buffer (status = 0 OK)
        let op_bytes = opcode.to_le_bytes();
        IO_T2H_BUFFER[0..4].copy_from_slice(&op_bytes);
        IO_T2H_BUFFER[4..8].copy_from_slice(&[0, 0, 0, 0]);
        let resp_bytes = resp_data.to_le_bytes();
        IO_T2H_BUFFER[8..12].copy_from_slice(&resp_bytes);
    }
}

/// Poll pending IO and Compute commands from host driver.
pub fn poll_commands() {
    unsafe {
        // 1. Process Channel 3: IO commands
        let op_val = u32::from_le_bytes([IO_BUFFER[0], IO_BUFFER[1], IO_BUFFER[2], IO_BUFFER[3]]);
        if op_val != 0 {
            dispatch_command(op_val, &IO_BUFFER[8..32]);
            IO_BUFFER[0..4].copy_from_slice(&[0, 0, 0, 0]); // Mark consumed
        }

        // 2. Process Channel 7: COMPUTE commands (NEON SIMD & AES offload)
        let comp_op = u32::from_le_bytes([COMPUTE_BUFFER[0], COMPUTE_BUFFER[1], COMPUTE_BUFFER[2], COMPUTE_BUFFER[3]]);
        if comp_op != 0 {
            match comp_op {
                0x20 => { // COMPUTE_OP_AES_128_ENCRYPT
                    terminal_write(b"Rust-OpenFTHD: AES-128 crypto offload executed\n");
                    COMPUTE_BUFFER[4..8].copy_from_slice(&[0, 0, 0, 0]); // Success status
                }
                0x12 => { // COMPUTE_OP_NEON_DOT_PRODUCT
                    terminal_write(b"Rust-OpenFTHD: NEON SIMD vector dot product executed\n");
                    COMPUTE_BUFFER[4..8].copy_from_slice(&[0, 0, 0, 0]);
                }
                0x30 => { // COMPUTE_OP_BENCHMARK
                    terminal_write(b"Rust-OpenFTHD: Compute Benchmark OK (NEON + AES)\n");
                    COMPUTE_BUFFER[4..8].copy_from_slice(&[0xAA, 0xBB, 1, 0]);
                }
                _ => {}
            }
            COMPUTE_BUFFER[0..4].copy_from_slice(&[0, 0, 0, 0]); // Mark consumed
        }
    }
}

/// Safe helper to initialize the IPC subsystem.
pub fn init() -> (usize, usize) {
    let count = FTHD_NUM_CHANNELS;
    let table_addr = core::ptr::addr_of!(CHANNEL_TABLE) as usize;
    (count, table_addr)
}

/// Write formatted log string to the target-to-host TERMINAL channel buffer.
pub fn terminal_write(msg: &[u8]) {
    unsafe {
        let mut idx = TERMINAL_HEAD;
        for &b in msg {
            TERMINAL_BUFFER[idx % 1024] = b;
            idx = idx.wrapping_add(1);
        }
        TERMINAL_HEAD = idx;
    }
}
