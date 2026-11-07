/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * ARMv7 Exception Vectors & Early Initialization.
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

    .syntax unified
    .arch armv7-a
    .arm

    .section .vectors, "ax"
    .global _start
    .global _vector_table

_vector_table:
_start:
    b   reset_handler           /* 0x00: Reset */
    b   hang                    /* 0x04: Undefined instruction */
    b   hang                    /* 0x08: Software interrupt */
    b   hang                    /* 0x0c: Prefetch abort */
    b   hang                    /* 0x10: Data abort */
    b   hang                    /* 0x14: Reserved */
    b   hang                    /* 0x18: IRQ */
    b   hang                    /* 0x1c: FIQ */

    /* BCM1570 Bootloader Identification Block at 0x40 */
    .org 0x40
    .ascii "FFW Application\0"
    .ascii "See next page   "
    .ascii "for information\0"

    /* BCM1570 ROM Page-1 Header at 0x1000 */
    .org 0x1000
    .word 0x00000000            /* 0x1000: Magic / Flags */
    .word 0x00800000            /* 0x1004: Channel Description Table Base in DRAM */
    .word 0x00800000            /* 0x1008: Mailbox Queue Base */
    .word 0x07800000            /* 0x100c: Memory Upper Limit (120 MB) */

    .org 0x1010
    .ascii "OpenFTHD (C) 2026 Carlos Alberto GPL-2.0\0"
    .org 0x1040
    .ascii "adc\0"
    .org 0x1050
    .ascii "release\0"
    .org 0x1060
    .ascii "Nov  7 2026\008:48:55\0"
    .org 0x1080
    .ascii "S2ISP-01.43.00\0"
    .org 0x1090
    .ascii "H4ISPAPPLE\0H4ISPCD\0ffw\0"
    .org 0x10b0
    .ascii "11536\04081\04143\0"
    .org 0x10d0
    .ascii "iPhone OS\0"
    .org 0x10e0
    .ascii "7.0.3\0"
    .org 0x10f0
    .ascii "clang/clang++\0"

    .org 0x1100
    .word 0x00300000            /* 0x1100: Firmware Requested Heap Size (3072 KB) */

    .org 0x1104
reset_handler:
    /* 1. Disable interrupts (IRQ and FIQ) */
    cpsid   if

    /* 2. Configure ACTLR: clear bit 1 (L2 prefetch hint) */
    mrc     p15, 0, r0, cr1, cr0, 1
    bic     r0, r0, #2
    mcr     p15, 0, r0, cr1, cr0, 1
    isb

    /* 3. Invalidate Instruction Cache (ICIALLU) & Branch Predictors (BPIALL) */
    mov     r0, #0
    mcr     p15, 0, r0, cr7, cr5, 0
    mcr     p15, 0, r0, cr7, cr5, 6
    isb

    /* 4. Configure SCTLR: Disable MMU, D-Cache, High vectors, keep I-Cache disabled */
    mrc     p15, 0, r0, cr1, cr0, 0
    bic     r0, r0, #0x2300     /* Clear bits 13 (V), 9 (ROM), 8 (System) */
    bic     r0, r0, #(1 << 12)  /* Clear bit 12 (I-Cache) */
    bic     r0, r0, #5          /* Clear bits 2 (D-Cache), 0 (MMU) */
    orr     r0, r0, #2          /* Alignment check (bit 1) */
    mcr     p15, 0, r0, cr1, cr0, 0
    isb

    /* 5. Set Vector Base Address Register (VBAR = 0x00000000) */
    mov     r0, #0
    mcr     p15, 0, r0, cr12, cr0, 0
    isb

    /* 6. Clear IFSR & DFSR Fault Status Registers */
    mov     r0, #0
    mcr     p15, 0, r0, cr5, cr0, 1
    mcr     p15, 0, r0, cr5, cr0, 0

    /* 7. Enable CP10 and CP11 (VFP and NEON) in CPACR */
    mrc     p15, 0, r0, cr1, cr0, 2
    orr     r0, r0, #(0xf << 20)
    mcr     p15, 0, r0, cr1, cr0, 2
    isb

    /* 8. Enable VFP / NEON in FPEXC (bit 30 = EN) */
    mov     r0, #(1 << 30)
    vmsr    fpexc, r0

    /* 9. Set Stack Pointer to safe DRAM location matching silicon expectation (0x0015d808) */
    ldr     sp, =0x0015d808

    /* 10. Zero out .bss section */
    ldr     r0, =__bss_start__
    ldr     r1, =__bss_end__
    mov     r2, #0
1:
    cmp     r0, r1
    strlt   r2, [r0], #4
    blt     1b

    /* 11. Call main firmware runtime */
    bl      fthd_main

hang:
    wfi
    b       hang

undefined_handler:
swi_handler:
prefetch_abort_handler:
data_abort_handler:
reserved_handler:
irq_handler:
fiq_handler:
    b       hang
