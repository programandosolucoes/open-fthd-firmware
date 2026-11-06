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
