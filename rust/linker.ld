/* SPDX-License-Identifier: GPL-2.0-only
 * Linker script for OpenFTHD Bare-Metal ARMv7 Firmware (Rust implementation)
 */

ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 2M
}

SECTIONS
{
    . = 0x00000000;

    .vector : {
        KEEP(*(.vector))
    } > RAM

    .text : {
        *(.text*)
    } > RAM

    .rodata : {
        *(.rodata*)
    } > RAM

    .data : {
        *(.data*)
    } > RAM

    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    . = ALIGN(8);
    __stack_bottom = .;
    . += 0x10000; /* 64 KB Stack */
    __stack_top = .;

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.comment)
        *(.note*)
    }
}
