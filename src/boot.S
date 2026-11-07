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
    b   undefined_handler       /* 0x04: Undefined instruction */
    b   swi_handler             /* 0x08: Software interrupt */
    b   prefetch_abort_handler  /* 0x0c: Prefetch abort */
    b   data_abort_handler      /* 0x10: Data abort */
    b   reserved_handler        /* 0x14: Reserved */
    b   irq_handler             /* 0x18: IRQ */
    b   fiq_handler             /* 0x1c: FIQ */

    /* Identification Banner at 0x40 (Interoperability check) */
    .org 0x40
    .ascii "OpenFTHD Clean-Room Firmware v0.1\0"
    .align 4

reset_handler:
    /* Disable interrupts (IRQ and FIQ) */
    cpsid   if

    /* Initialize CP15 System Control Register */
    mrc     p15, 0, r0, c1, c0, 0
    bic     r0, r0, #(1 << 0)   /* Disable MMU */
    bic     r0, r0, #(1 << 2)   /* Disable D-Cache */
    orr     r0, r0, #(1 << 12)  /* Enable I-Cache */
    mcr     p15, 0, r0, c1, c0, 0
    isb

    /* Set Vector Base Address Register (VBAR = 0x00000000) */
    mov     r0, #0
    mcr     p15, 0, r0, c12, c0, 0
    isb

    /* Enable CP10 and CP11 (VFP and NEON) in CPACR */
    mrc     p15, 0, r0, c1, c0, 2
    orr     r0, r0, #(0xf << 20)
    mcr     p15, 0, r0, c1, c0, 2
    isb

    /* Enable VFP / NEON in FPEXC (bit 30 = EN) */
    mov     r0, #(1 << 30)
    vmsr    fpexc, r0

    /* Set Stack Pointer to top of allocated stack */
    ldr     sp, =_stack_top

    /* Zero out .bss section */
    ldr     r0, =__bss_start__
    ldr     r1, =__bss_end__
    mov     r2, #0
1:
    cmp     r0, r1
    strlt   r2, [r0], #4
    blt     1b

    /* Call main firmware runtime */
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
