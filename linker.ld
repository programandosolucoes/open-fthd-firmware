/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD Linker Script
 *
 * Mapped for Broadcom BCM1570 SRAM starting at 0x00000000
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 2M
}

SECTIONS
{
    . = 0x00000000;

    .vectors : {
        KEEP(*(.vectors))
    } > RAM

    .text : {
        *(.text*)
        *(.rodata*)
        *(.data*)
    } > RAM

    . = ALIGN(4);
    __bss_start__ = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    } > RAM
    . = ALIGN(4);
    __bss_end__ = .;

    /* Reserve 64KB for stack at top */
    . = ALIGN(8);
    .stack : {
        . = . + 0x10000;
        _stack_top = .;
    } > RAM

    _end = .;
}
