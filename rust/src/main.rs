// SPDX-License-Identifier: GPL-2.0-only
//! OpenFTHD - Clean-Room Bare-Metal Rust Firmware for Broadcom BCM1570 FaceTime HD.

#![no_std]
#![no_main]
#![allow(static_mut_refs)]

mod registers;
mod ipc;
mod sensor;
mod isp;

use core::panic::PanicInfo;
use registers::{
    MmioRegister,
    ISP_FW_CHAN_CTRL,
    ISP_FW_SIZE,
    ISP_FW_HEAP_SIZE,
};
use sensor::Ov5650Driver;

// Vector table and reset bootstrap in ARMv7 assembly
core::arch::global_asm!(
    r#"
    .section .vector, "ax"
    .global _start
    _start:
        b reset_handler
        b undef_handler
        b svc_handler
        b prefetch_abort
        b data_abort
        b .
        b irq_handler
        b fiq_handler

    reset_handler:
        /* Switch to Supervisor (SVC) mode and disable IRQ/FIQ */
        cpsid if, #0x13

        /* Setup bare-metal stack pointer */
        ldr sp, =__stack_top

        /* Zero out BSS section */
        ldr r0, =__bss_start
        ldr r1, =__bss_end
        mov r2, #0
    1:
        cmp r0, r1
        strlt r2, [r0], #4
        blt 1b

        /* Jump to Rust main entry */
        bl rust_entry

    halt:
        wfe
        b halt

    undef_handler:  b undef_handler
    svc_handler:    b svc_handler
    prefetch_abort: b prefetch_abort
    data_abort:     b data_abort
    irq_handler:    b irq_handler
    fiq_handler:    b fiq_handler
    "#
);

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {
        unsafe {
            core::arch::asm!("wfe");
        }
    }
}

/// Main Rust firmware entry point called directly from reset vector.
#[no_mangle]
pub extern "C" fn rust_entry() -> ! {
    // 1. Initialize IPC subsystem and channel tables
    let (chan_count, chan_table_addr) = ipc::init();

    // 2. Publish channels to PCIe Mailbox
    let reg_chan_ctrl = MmioRegister::new(ISP_FW_CHAN_CTRL);
    reg_chan_ctrl.publish_channel_table(chan_count as u16, chan_table_addr as u16);

    // 3. Negotiate 2 MB DDR Heap with Host Linux Kernel
    let reg_heap_size = MmioRegister::new(ISP_FW_HEAP_SIZE);
    reg_heap_size.write(2 * 1024 * 1024);

    // 4. Report firmware size (e.g. 2 KB footprint)
    let reg_fw_size = MmioRegister::new(ISP_FW_SIZE);
    reg_fw_size.write(2048);

    // 5. Initialize Sensor State Machine
    let mut sensor = Ov5650Driver::new();
    let _ = sensor.set_mode(0); // 720p@30 default

    // 6. Write startup greeting to Host Terminal channel
    ipc::terminal_write(b"OpenFTHD (Rust Bare-Metal v0.1.0) ready\n");

    // 7. Active command processing loop
    loop {
        ipc::poll_commands();
        unsafe {
            core::arch::asm!("wfe");
        }
    }
}
