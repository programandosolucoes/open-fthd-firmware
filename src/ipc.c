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
#include "bcm1570_hw.h"

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
}

uint32_t ipc_get_channel_table_addr(void)
{
    return (uint32_t)&g_channels[0];
}

uint32_t ipc_get_num_channels(void)
{
    return MAX_CHANNELS;
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
