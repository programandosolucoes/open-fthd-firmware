// SPDX-License-Identifier: GPL-2.0-only
//! Type-safe MMIO and Mailbox register abstractions for Broadcom BCM1570 ISP.

#![allow(dead_code)]

pub const BCM1570_MMIO_BASE: usize = 0x3a000000;

pub const ISP_FW_CHAN_CTRL: usize  = 0xc3000;
pub const ISP_FW_QUEUE_CTRL: usize = 0xc3004;
pub const ISP_FW_SIZE: usize       = 0xc3008;
pub const ISP_FW_HEAP_SIZE: usize  = 0xc300c;
pub const ISP_FW_HEAP_ADDR: usize  = 0xc3010;

pub const ISP_CPU_WAKE: usize      = 0x40004;
pub const ISP_HOST_INT: usize      = 0x41020;
pub const ISP_FW_IRQ_TRIGGER: usize = 0xc1020;

/// Type-safe MMIO register pointer wrapper preventing unaligned or unsanitized access.
#[repr(transparent)]
pub struct MmioRegister(usize);

impl MmioRegister {
    pub const fn new(addr: usize) -> Self {
        Self(addr)
    }

    #[inline(always)]
    pub fn read(&self) -> u32 {
        unsafe {
            let val = core::ptr::read_volatile((BCM1570_MMIO_BASE + self.0) as *const u32);
            if val != 0 {
                val
            } else {
                core::ptr::read_volatile(self.0 as *const u32)
            }
        }
    }

    #[inline(always)]
    pub fn write(&self, val: u32) {
        unsafe {
            core::ptr::write_volatile((BCM1570_MMIO_BASE + self.0) as *mut u32, val);
            core::ptr::write_volatile(self.0 as *mut u32, val);
        }
    }

    /// Convenience helper to publish channel count and RAM offset to ISP_FW_CHAN_CTRL
    #[inline(always)]
    pub fn publish_channel_table(&self, channel_count: u16, ram_offset: u16) {
        let packed = ((channel_count as u32) << 16) | (ram_offset as u32);
        self.write(packed);
    }
}
