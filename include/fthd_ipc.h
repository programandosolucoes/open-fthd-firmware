/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * IPC Communication Interface definitions.
 * Derived from the public host driver interface (patjak/bcwc_pcie)
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#ifndef _OPENFTHD_IPC_H
#define _OPENFTHD_IPC_H

#include <stdint.h>

#define MAX_CHANNELS            7
#define CHANNEL_NAME_MAX_LEN    32
#define RINGBUF_ENTRY_SIZE      16

/* Standard Channel Names expected by the Linux bcwc_pcie host driver */
#define CHAN_NAME_TERMINAL      "TERMINAL"
#define CHAN_NAME_DEBUG         "DEBUG"
#define CHAN_NAME_SHAREDMALLOC  "SHAREDMALLOC"
#define CHAN_NAME_IO            "IO"
#define CHAN_NAME_BUF_H2T       "BUF_H2T"
#define CHAN_NAME_BUF_T2H       "BUF_T2H"
#define CHAN_NAME_IO_T2H        "IO_T2H"

/* Extended Compute / Hardware Offload Channel (for NEON SIMD & DSP acceleration) */
#define CHAN_NAME_COMPUTE       "COMPUTE"

/* Channel Types */
enum fthd_channel_type {
    CHAN_TYPE_STREAM = 1,
    CHAN_TYPE_RINGBUF = 2,
};

/* Channel Description Table Entry (256 bytes per entry in device RAM) */
struct __attribute__((packed)) fthd_channel_info {
    char name[CHANNEL_NAME_MAX_LEN];
    uint32_t type;
    uint32_t source;
    uint32_t size;
    uint32_t offset;
    uint8_t  reserved[212];
};

/* Ringbuffer Entry Structure */
struct __attribute__((packed)) fthd_ringbuf_entry {
    uint32_t address_flags;     /* Memory buffer address + flags */
    uint32_t request_size;      /* Request length in bytes */
    uint32_t response_size;     /* Response length in bytes */
    uint32_t sequence;          /* Sequence tag / status */
};

/* Command Header Structure */
struct __attribute__((packed)) fthd_cmd_header {
    uint32_t opcode;
    uint32_t status;
};

/* IPC Command Opcodes (CISP_CMD_*) */
enum fthd_isp_cmds {
    CISP_CMD_START                          = 0x0,
    CISP_CMD_STOP                           = 0x1,
    CISP_CMD_RESET                          = 0x2,
    CISP_CMD_CONFIG_GET                     = 0x3,
    CISP_CMD_PRINT_ENABLE                   = 0x4,
    CISP_CMD_REG_FILE_LOAD                  = 0x5,
    CISP_CMD_BUILDINFO                      = 0x6,
    CISP_CMD_TIMEPROFILE_START              = 0x7,
    CISP_CMD_TIMEPROFILE_STOP               = 0x8,
    CISP_CMD_TIMEPROFILE_SHOW               = 0x9,
    CISP_CMD_POWER_DOWN                     = 0xa,
    CISP_CMD_CH_START                       = 0x100,
    CISP_CMD_CH_STOP                        = 0x101,
    CISP_CMD_CH_RESET                       = 0x102,
    CISP_CMD_CH_STANDBY                     = 0x103,
    CISP_CMD_CH_BUFFER_RETURN               = 0x104,
    CISP_CMD_CH_CAMERA_CONFIG_CURRENT_GET   = 0x105,
    CISP_CMD_CH_CAMERA_CONFIG_GET           = 0x106,
    CISP_CMD_CH_CAMERA_CONFIG_SELECT        = 0x107,
    CISP_CMD_CH_RAW_FRAME_PROCESS           = 0x108,
    CISP_CMD_CH_RAW_FRAME_PROCESS_START     = 0x109,
    CISP_CMD_CH_RAW_FRAME_PROCESS_STOP      = 0x10a,
    CISP_CMD_CH_I2C_READ                    = 0x10b,
    CISP_CMD_CH_I2C_WRITE                   = 0x10c,
    CISP_CMD_CH_INFO_GET                    = 0x10d,
    CISP_CMD_CH_CAPTURE_MODE_SET            = 0x112,
    CISP_CMD_CH_CAMERA_MIPI_FREQ_SELECT     = 0x11b,
    CISP_CMD_CH_AE_START                    = 0x200,
    CISP_CMD_CH_AE_STOP                     = 0x201,
};

#endif /* _OPENFTHD_IPC_H */
