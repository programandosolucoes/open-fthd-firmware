// SPDX-License-Identifier: GPL-2.0-only
//! Host <-> ISP Inter-Process Communication (IPC) Channels and Ringbuffers.

#![allow(dead_code)]

pub const FTHD_NUM_CHANNELS: usize = 7;

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
];

/// Ringbuffer memory area for the terminal output.
static mut TERMINAL_BUFFER: [u8; 1024] = [0; 1024];
static mut TERMINAL_HEAD: usize = 0;

/// Ringbuffer memory area for IO requests (Channel 3: IO).
static mut IO_BUFFER: [u8; 1024] = [0; 1024];

/// Ringbuffer memory area for IO responses (Channel 6: IO_T2H).
static mut IO_T2H_BUFFER: [u8; 1024] = [0; 1024];

/// Firmware internal ISP state machine.
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
        match opcode {
            0x100 => { // CH_START
                ISP_STATE.streaming_active = true;
                terminal_write(b"Rust-OpenFTHD: CH_START streaming active\n");
            }
            0x101 => { // CH_STOP
                ISP_STATE.streaming_active = false;
                terminal_write(b"Rust-OpenFTHD: CH_STOP standby\n");
            }
            0x801 => { // CH_CROP_SET
                if payload.len() >= 20 {
                    ISP_STATE.crop_x = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                    ISP_STATE.crop_y = u32::from_le_bytes([payload[8], payload[9], payload[10], payload[11]]);
                    ISP_STATE.crop_width = u32::from_le_bytes([payload[12], payload[13], payload[14], payload[15]]);
                    ISP_STATE.crop_height = u32::from_le_bytes([payload[16], payload[17], payload[18], payload[19]]);
                    terminal_write(b"Rust-OpenFTHD: ePTZ hardware crop applied\n");
                }
            }
            0xb01 => { // CH_OUTPUT_CONFIG_SET
                if payload.len() >= 12 {
                    ISP_STATE.output_width = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                    ISP_STATE.output_height = u32::from_le_bytes([payload[8], payload[9], payload[10], payload[11]]);
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
            0x300 => { // AWB_START
                ISP_STATE.awb_enabled = true;
            }
            0x301 => { // AWB_STOP
                ISP_STATE.awb_enabled = false;
            }
            0x21e => { // AE_WINDOW_PARAM_SET
                if payload.len() >= 8 {
                    ISP_STATE.ae_metering = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                }
            }
            0xc00 => { // DRC_START
                ISP_STATE.drc_enabled = true;
                terminal_write(b"Rust-OpenFTHD: DRC / HDR enabled\n");
            }
            0xc01 => { // DRC_STOP
                ISP_STATE.drc_enabled = false;
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
    }
}

/// Poll pending IO commands from host driver.
pub fn poll_commands() {
    unsafe {
        let op_val = u32::from_le_bytes([IO_BUFFER[0], IO_BUFFER[1], IO_BUFFER[2], IO_BUFFER[3]]);
        if op_val != 0 {
            dispatch_command(op_val, &IO_BUFFER[8..32]);
            IO_BUFFER[0..4].copy_from_slice(&[0, 0, 0, 0]); // Mark consumed
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
