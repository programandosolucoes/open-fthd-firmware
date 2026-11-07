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

    /* Channel Management Commands (0x100+) */
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
    CISP_CMD_CH_BUFFER_RECYCLE_MODE_SET     = 0x10e,
    CISP_CMD_CH_BUFFER_RECYCLE_START        = 0x10f,
    CISP_CMD_CH_CAPTURE_MODE_SET            = 0x112,
    CISP_CMD_CH_CAMERA_MIPI_FREQ_SELECT     = 0x11b,

    /* Auto-Exposure Controls (0x200+) */
    CISP_CMD_CH_AE_START                    = 0x200,
    CISP_CMD_CH_AE_STOP                     = 0x201,
    CISP_CMD_CH_AE_FRAME_RATE_MAX_SET       = 0x208,
    CISP_CMD_CH_AE_FRAME_RATE_MIN_SET       = 0x20a,
    CISP_CMD_CH_AE_WINDOW_PARAM_SET         = 0x21e, /* Photometry Metering */

    /* Auto White Balance (0x300+) */
    CISP_CMD_CH_AWB_START                   = 0x300,
    CISP_CMD_CH_AWB_STOP                    = 0x301,

    /* Status LED & Flash Controls (0x600+) */
    CISP_CMD_CH_LED_TORCH_OFF               = 0x602,
    CISP_CMD_CH_LED_TORCH_ON                = 0x603,
    CISP_CMD_CH_STATUS_LED_DEBUG_SET        = 0x608,

    /* Hardware Cropping / ePTZ (0x800+) */
    CISP_CMD_CH_CROP_GET                    = 0x800,
    CISP_CMD_CH_CROP_SET                    = 0x801,

    /* Color & Post-Processing (0xa00+) */
    CISP_CMD_CH_COLOR_SATURATION_SET        = 0xa01,

    /* Output Resolution & Format (0xb00+) */
    CISP_CMD_CH_OUTPUT_CONFIG_GET           = 0xb00,
    CISP_CMD_CH_OUTPUT_CONFIG_SET           = 0xb01,
    CISP_CMD_CH_SCALER_BRIGHTNESS_SET       = 0xb02,
    CISP_CMD_CH_SCALER_CONTRAST_SET         = 0xb03,
    CISP_CMD_CH_SCALER_SATURATION_SET       = 0xb04,
    CISP_CMD_CH_SCALER_HUE_SET              = 0xb05,

    /* Dynamic Range Compression / HDR (0xc00+) */
    CISP_CMD_CH_DRC_START                   = 0xc00,
    CISP_CMD_CH_DRC_STOP                    = 0xc01,
};

/* Command Payload Structures matching Host Linux Driver */
struct __attribute__((packed)) fthd_cmd_crop_set {
    uint32_t channel;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

struct __attribute__((packed)) fthd_cmd_output_config {
    uint32_t channel;
    uint32_t x1;
    uint32_t y1;
    uint32_t unknown3;
    uint32_t pixelformat; /* 1 = YUYV */
    uint32_t x2;
    uint32_t x3;
};

struct __attribute__((packed)) fthd_cmd_int_param {
    uint32_t channel;
    int32_t  value;
};

/* Firmware Internal ISP State */
struct fthd_isp_state {
    uint32_t streaming_active;
    uint32_t crop_x;
    uint32_t crop_y;
    uint32_t crop_width;
    uint32_t crop_height;
    uint32_t output_width;
    uint32_t output_height;
    uint32_t pixelformat;
    int32_t  brightness;
    int32_t  contrast;
    int32_t  saturation;
    int32_t  hue;
    uint32_t awb_enabled;
    uint32_t ae_metering;
    uint32_t drc_enabled;
    uint32_t led_mode;
    uint32_t frame_sequence;
};

extern struct fthd_isp_state g_isp_state;
extern int ipc_dispatch_command(uint32_t opcode, const void *payload, uint32_t len);
extern void ipc_poll_commands(void);

#endif /* _OPENFTHD_IPC_H */
