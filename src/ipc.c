/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * IPC Channel and Ringbuffer Management.
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#include "fthd_ipc.h"
#include "fthd_compute.h"
#include "bcm1570_hw.h"

void ipc_terminal_write(const char *msg);

/* Memory regions reserved in internal SRAM for IPC buffers */
#define IPC_BASE_OFFSET         0x80000
#define RINGBUF_SIZE            1024

static struct fthd_channel_info g_channels[MAX_CHANNELS];
static uint8_t g_ringbuffers[MAX_CHANNELS][RINGBUF_SIZE];

static void fthd_strncpy(char *dst, const char *src, int n)
{
    int i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void ipc_init(void)
{
    /* 1. Clear channel structures */
    for (int i = 0; i < MAX_CHANNELS; i++) {
        uint8_t *p = (uint8_t *)&g_channels[i];
        for (int j = 0; j < (int)sizeof(struct fthd_channel_info); j++) {
            p[j] = 0;
        }
    }

    /* 2. Configure standard expected channels */
    fthd_strncpy(g_channels[0].name, CHAN_NAME_TERMINAL, CHANNEL_NAME_MAX_LEN);
    g_channels[0].type = CHAN_TYPE_STREAM;
    g_channels[0].size = RINGBUF_SIZE;
    g_channels[0].offset = (uint32_t)&g_ringbuffers[0];

    fthd_strncpy(g_channels[1].name, CHAN_NAME_DEBUG, CHANNEL_NAME_MAX_LEN);
    g_channels[1].type = CHAN_TYPE_STREAM;
    g_channels[1].size = RINGBUF_SIZE;
    g_channels[1].offset = (uint32_t)&g_ringbuffers[1];

    fthd_strncpy(g_channels[2].name, CHAN_NAME_SHAREDMALLOC, CHANNEL_NAME_MAX_LEN);
    g_channels[2].type = CHAN_TYPE_RINGBUF;
    g_channels[2].size = RINGBUF_SIZE;
    g_channels[2].offset = (uint32_t)&g_ringbuffers[2];

    fthd_strncpy(g_channels[3].name, CHAN_NAME_IO, CHANNEL_NAME_MAX_LEN);
    g_channels[3].type = CHAN_TYPE_RINGBUF;
    g_channels[3].size = RINGBUF_SIZE;
    g_channels[3].offset = (uint32_t)&g_ringbuffers[3];

    fthd_strncpy(g_channels[4].name, CHAN_NAME_BUF_H2T, CHANNEL_NAME_MAX_LEN);
    g_channels[4].type = CHAN_TYPE_RINGBUF;
    g_channels[4].size = RINGBUF_SIZE;
    g_channels[4].offset = (uint32_t)&g_ringbuffers[4];

    fthd_strncpy(g_channels[5].name, CHAN_NAME_BUF_T2H, CHANNEL_NAME_MAX_LEN);
    g_channels[5].type = CHAN_TYPE_RINGBUF;
    g_channels[5].size = RINGBUF_SIZE;
    g_channels[5].offset = (uint32_t)&g_ringbuffers[5];

    fthd_strncpy(g_channels[6].name, CHAN_NAME_IO_T2H, CHANNEL_NAME_MAX_LEN);
    g_channels[6].type = CHAN_TYPE_RINGBUF;
    g_channels[6].size = RINGBUF_SIZE;
    g_channels[6].offset = (uint32_t)&g_ringbuffers[6];

    fthd_strncpy(g_channels[7].name, CHAN_NAME_COMPUTE, CHANNEL_NAME_MAX_LEN);
    g_channels[7].type = CHAN_TYPE_RINGBUF;
    g_channels[7].size = RINGBUF_SIZE;
    g_channels[7].offset = (uint32_t)&g_ringbuffers[7];
}

uint32_t ipc_get_channel_table_addr(void)
{
    return (uint32_t)&g_channels[0];
}

uint32_t ipc_get_num_channels(void)
{
    return MAX_CHANNELS;
}

struct fthd_isp_state g_isp_state = {
    .streaming_active = 0,
    .crop_x = 0,
    .crop_y = 0,
    .crop_width = 1280,
    .crop_height = 720,
    .output_width = 1280,
    .output_height = 720,
    .pixelformat = 1, /* YUYV */
    .brightness = 128,
    .contrast = 128,
    .saturation = 128,
    .hue = 128,
    .awb_enabled = 1,
    .ae_metering = 3, /* Matrix default */
    .drc_enabled = 0,
    .led_mode = 0,    /* Auto */
    .frame_sequence = 0,
};

extern int sensor_ov5650_init(void);

int ipc_dispatch_command(uint32_t opcode, const void *payload, uint32_t len)
{
    (void)len;

    switch (opcode) {
    case CISP_CMD_CH_START:
        g_isp_state.streaming_active = 1;
        sensor_ov5650_init();
        ipc_terminal_write("CH_START: Sensor pipeline active\n");
        break;

    case CISP_CMD_CH_STOP:
        g_isp_state.streaming_active = 0;
        ipc_terminal_write("CH_STOP: Standby mode\n");
        break;

    case CISP_CMD_CH_CROP_SET:
        if (payload) {
            const struct fthd_cmd_crop_set *crop = (const struct fthd_cmd_crop_set *)payload;
            g_isp_state.crop_x = crop->x;
            g_isp_state.crop_y = crop->y;
            g_isp_state.crop_width = crop->width;
            g_isp_state.crop_height = crop->height;
            ipc_terminal_write("CROP_SET: Applied ePTZ window\n");
        }
        break;

    case CISP_CMD_CH_OUTPUT_CONFIG_SET:
        if (payload) {
            const struct fthd_cmd_output_config *out = (const struct fthd_cmd_output_config *)payload;
            g_isp_state.output_width = out->x1;
            g_isp_state.output_height = out->y1;
            g_isp_state.pixelformat = out->pixelformat;
            ipc_terminal_write("OUTPUT_CONFIG_SET: Resolution updated\n");
        }
        break;

    case CISP_CMD_CH_SCALER_BRIGHTNESS_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            g_isp_state.brightness = param->value;
        }
        break;

    case CISP_CMD_CH_SCALER_CONTRAST_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            g_isp_state.contrast = param->value;
        }
        break;

    case CISP_CMD_CH_SCALER_SATURATION_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            g_isp_state.saturation = param->value;
        }
        break;

    case CISP_CMD_CH_SCALER_HUE_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            g_isp_state.hue = param->value;
        }
        break;

    case CISP_CMD_CH_AWB_START:
        g_isp_state.awb_enabled = 1;
        break;

    case CISP_CMD_CH_AWB_STOP:
        g_isp_state.awb_enabled = 0;
        break;

    case CISP_CMD_CH_AE_WINDOW_PARAM_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            g_isp_state.ae_metering = (uint32_t)param->value;
        }
        break;

    case CISP_CMD_CH_DRC_START:
        g_isp_state.drc_enabled = 1;
        ipc_terminal_write("DRC_START: Tone curve adaptation enabled\n");
        break;

    case CISP_CMD_CH_DRC_STOP:
        g_isp_state.drc_enabled = 0;
        break;

    case CISP_CMD_CH_STATUS_LED_DEBUG_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            g_isp_state.led_mode = (uint32_t)param->value;
        }
        break;

    default:
        break;
    }

    /* Acknowledge command on IO_T2H ringbuffer */
    volatile struct fthd_cmd_header *resp = (volatile struct fthd_cmd_header *)&g_ringbuffers[6][0];
    resp->opcode = opcode;
    resp->status = 0; /* Success */

    return 0;
}

void ipc_poll_commands(void)
{
    /* 1. Check IO ringbuffer (Channel 3) for pending command header */
    volatile struct fthd_cmd_header *hdr = (volatile struct fthd_cmd_header *)&g_ringbuffers[3][0];
    if (hdr->opcode != 0) {
        uint32_t op = hdr->opcode;
        const void *payload = (const void *)&g_ringbuffers[3][sizeof(struct fthd_cmd_header)];
        ipc_dispatch_command(op, payload, 32);
        hdr->opcode = 0; /* Clear after processing */
    }

    /* 2. Check COMPUTE ringbuffer (Channel 7) for NEON SIMD / AES requests */
    volatile struct fthd_compute_header *comp_hdr = (volatile struct fthd_compute_header *)&g_ringbuffers[7][0];
    if (comp_hdr->opcode != 0) {
        uint32_t resp_len = 0;
        uint8_t resp_buf[128];
        const void *comp_payload = (const void *)&g_ringbuffers[7][sizeof(struct fthd_compute_header)];

        compute_dispatch((const struct fthd_compute_header *)comp_hdr, comp_payload, resp_buf, &resp_len);

        /* Write result back to COMPUTE ringbuffer */
        uint8_t *dest = (uint8_t *)&g_ringbuffers[7][0];
        for (uint32_t i = 0; i < resp_len && i < RINGBUF_SIZE; i++) {
            dest[i] = resp_buf[i];
        }
    }
}

void ipc_terminal_write(const char *msg)
{
    volatile char *buf = (volatile char *)&g_ringbuffers[0][0];
    int idx = 0;
    while (msg[idx] != '\0' && idx < RINGBUF_SIZE - 1) {
        buf[idx] = msg[idx];
        idx++;
    }
    buf[idx] = '\0';
}
