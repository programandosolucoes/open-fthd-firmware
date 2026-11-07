/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * IPC Channel and Ringbuffer Management & Hardware CISP Dispatcher.
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#include "fthd_ipc.h"
#include "fthd_isp_pipeline.h"
#include "fthd_compute.h"
#include "ov5650.h"
#include "bcm1570_hw.h"

void ipc_terminal_write(const char *msg);

/* Memory regions reserved in internal SRAM for IPC buffers */
#define RINGBUF_ENTRIES         64
#define RINGBUF_SIZE            (RINGBUF_ENTRIES * RINGBUF_ENTRY_SIZE)

static struct fthd_channel_info g_channels[MAX_CHANNELS] __attribute__((aligned(256)));
static uint8_t g_ringbuffers[MAX_CHANNELS][RINGBUF_SIZE] __attribute__((aligned(256)));

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
    /* 1. Copy standard channel names expected by Linux bcwc_pcie host driver */
    fthd_strncpy(g_channels[0].name, CHAN_NAME_TERMINAL, CHANNEL_NAME_MAX_LEN);
    fthd_strncpy(g_channels[1].name, CHAN_NAME_DEBUG, CHANNEL_NAME_MAX_LEN);
    fthd_strncpy(g_channels[2].name, CHAN_NAME_SHAREDMALLOC, CHANNEL_NAME_MAX_LEN);
    fthd_strncpy(g_channels[3].name, CHAN_NAME_IO, CHANNEL_NAME_MAX_LEN);
    fthd_strncpy(g_channels[4].name, CHAN_NAME_BUF_H2T, CHANNEL_NAME_MAX_LEN);
    fthd_strncpy(g_channels[5].name, CHAN_NAME_BUF_T2H, CHANNEL_NAME_MAX_LEN);
    fthd_strncpy(g_channels[6].name, CHAN_NAME_IO_T2H, CHANNEL_NAME_MAX_LEN);
    fthd_strncpy(g_channels[7].name, CHAN_NAME_COMPUTE, CHANNEL_NAME_MAX_LEN);

    /* 2. Configure channel parameters matching Linux driver fthd_ringbuf types */
    g_channels[0].type = 1; /* CHAN_TYPE_T2H */
    g_channels[0].source = 0;
    g_channels[0].size = RINGBUF_ENTRIES;
    g_channels[0].offset = (uint32_t)&g_ringbuffers[0];

    g_channels[1].type = 1; /* CHAN_TYPE_T2H */
    g_channels[1].source = 1;
    g_channels[1].size = RINGBUF_ENTRIES;
    g_channels[1].offset = (uint32_t)&g_ringbuffers[1];

    g_channels[2].type = 2; /* CHAN_TYPE_UNIDIR */
    g_channels[2].source = 2;
    g_channels[2].size = RINGBUF_ENTRIES;
    g_channels[2].offset = (uint32_t)&g_ringbuffers[2];

    g_channels[3].type = 0; /* CHAN_TYPE_H2T */
    g_channels[3].source = 3;
    g_channels[3].size = RINGBUF_ENTRIES;
    g_channels[3].offset = (uint32_t)&g_ringbuffers[3];

    g_channels[4].type = 0; /* CHAN_TYPE_H2T */
    g_channels[4].source = 0;
    g_channels[4].size = RINGBUF_ENTRIES;
    g_channels[4].offset = (uint32_t)&g_ringbuffers[4];

    g_channels[5].type = 1; /* CHAN_TYPE_T2H */
    g_channels[5].source = 1;
    g_channels[5].size = RINGBUF_ENTRIES;
    g_channels[5].offset = (uint32_t)&g_ringbuffers[5];

    g_channels[6].type = 1; /* CHAN_TYPE_T2H */
    g_channels[6].source = 2;
    g_channels[6].size = RINGBUF_ENTRIES;
    g_channels[6].offset = (uint32_t)&g_ringbuffers[6];

    g_channels[7].type = 0; /* CHAN_TYPE_H2T */
    g_channels[7].source = 3;
    g_channels[7].size = RINGBUF_ENTRIES;
    g_channels[7].offset = (uint32_t)&g_ringbuffers[7];

    /* 3. Mirror channel table to DRAM address 0x00800000 for PCIe host */
    volatile uint8_t *dst = (volatile uint8_t *)0x00800000;
    const uint8_t *src = (const uint8_t *)g_channels;
    for (uint32_t k = 0; k < sizeof(g_channels); k++) {
        dst[k] = src[k];
    }
}

uint32_t ipc_get_channel_table_addr(void)
{
    return 0x00800000;
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
    .brightness = 0,
    .contrast = 128,
    .saturation = 128,
    .hue = 0,
    .awb_enabled = 1,
    .ae_metering = 3, /* Matrix default */
    .drc_enabled = 0,
    .led_mode = 0,    /* Auto */
    .frame_sequence = 0,
};

int ipc_dispatch_command(uint32_t opcode, const void *payload, uint32_t len)
{
    (void)len;
    uint32_t resp_data = 0;

    switch (opcode) {
    /* 0. Host Driver Discovery & Calibration Handshake */
    case CISP_CMD_START:
    case CISP_CMD_RESET:
    case CISP_CMD_PRINT_ENABLE:
    case CISP_CMD_CONFIG_GET:
    case CISP_CMD_CH_SET_FILE_LOAD:
        break;

    case CISP_CMD_CH_INFO_GET:
        if (payload) {
            uint8_t *p = (uint8_t *)payload;
            /* sensorid0 at offset 0x10 = 0x5650 (OV5650) */
            *(uint16_t *)(p + 0x10) = 0x5650;
            /* sensorid1 at offset 0x18 = 0x1871 (MacBook Pro 11,2) */
            *(uint16_t *)(p + 0x18) = 0x1871;
            /* sensor_count at offset 0x58 = 1 */
            *(uint32_t *)(p + 0x58) = 1;
            /* camera module serial number string at offset 0x88 */
            fthd_strncpy((char *)(p + 0x88), "CC2600FTHD0001", 18);
        }
        break;

    case CISP_CMD_CH_CAMERA_CONFIG_GET:
        if (payload) {
            uint8_t *p = (uint8_t *)payload;
            /* Native sensor resolution: 1280x720 */
            p[8] = 1280 & 0xff;
            p[9] = 1280 >> 8;
            p[10] = 720 & 0xff;
            p[11] = 720 >> 8;
        }
        break;

    /* 1. Camera Lifecycle */
    case CISP_CMD_CH_START:
        g_isp_state.streaming_active = 1;
        sensor_ov5650_init();
        isp_pipeline_init();
        ipc_terminal_write("CH_START: Sensor & ISP Pipeline active\n");
        break;

    case CISP_CMD_CH_STOP:
        g_isp_state.streaming_active = 0;
        sensor_ov5650_power_off();
        ipc_terminal_write("CH_STOP: Standby mode\n");
        break;

    /* 2. Sensor Mode & Clock Configuration */
    case CISP_CMD_CH_CAMERA_CONFIG_SELECT:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            sensor_ov5650_set_mode((enum ov5650_mode_id)param->value);
            ipc_terminal_write("CONFIG_SELECT: Resolution mode set\n");
        }
        break;

    case CISP_CMD_CH_SENSOR_FRAME_RATE_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            sensor_ov5650_set_framerate((uint32_t)param->value);
        }
        break;

    case CISP_CMD_CH_SENSOR_POWER_ON:
        sensor_ov5650_power_on();
        break;

    case CISP_CMD_CH_SENSOR_POWER_OFF:
        sensor_ov5650_power_off();
        break;

    /* 3. ePTZ Cropping & Resolution Scaler */
    case CISP_CMD_CH_CROP_SET:
    case CISP_CMD_CH_SCALER_CROP_SET:
        if (payload) {
            const struct fthd_cmd_crop_set *crop = (const struct fthd_cmd_crop_set *)payload;
            g_isp_state.crop_x = crop->x;
            g_isp_state.crop_y = crop->y;
            g_isp_state.crop_width = crop->width;
            g_isp_state.crop_height = crop->height;
            isp_scaler_set_crop(crop->x, crop->y, crop->width, crop->height);
            ipc_terminal_write("CROP_SET: Applied ePTZ window\n");
        }
        break;

    case CISP_CMD_CH_OUTPUT_CONFIG_SET:
        if (payload) {
            const struct fthd_cmd_output_config *out = (const struct fthd_cmd_output_config *)payload;
            g_isp_state.output_width = out->x1;
            g_isp_state.output_height = out->y1;
            g_isp_state.pixelformat = out->pixelformat;
            isp_scaler_set_output(out->x1, out->y1, out->pixelformat);
            ipc_terminal_write("OUTPUT_CONFIG_SET: Resolution updated\n");
        }
        break;

    /* 4. Color Controls & Post-Processing */
    case CISP_CMD_CH_SCALER_BRIGHTNESS_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            g_isp_state.brightness = param->value;
            isp_color_set_params(g_isp_state.brightness, g_isp_state.contrast, g_isp_state.saturation, g_isp_state.hue);
        }
        break;

    case CISP_CMD_CH_SCALER_CONTRAST_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            g_isp_state.contrast = param->value;
            isp_color_set_params(g_isp_state.brightness, g_isp_state.contrast, g_isp_state.saturation, g_isp_state.hue);
        }
        break;

    case CISP_CMD_CH_SCALER_SATURATION_SET:
    case CISP_CMD_CH_COLOR_SATURATION_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            g_isp_state.saturation = param->value;
            isp_color_set_params(g_isp_state.brightness, g_isp_state.contrast, g_isp_state.saturation, g_isp_state.hue);
        }
        break;

    case CISP_CMD_CH_SCALER_HUE_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            g_isp_state.hue = param->value;
            isp_color_set_params(g_isp_state.brightness, g_isp_state.contrast, g_isp_state.saturation, g_isp_state.hue);
        }
        break;

    /* 5. 3A Auto-Exposure */
    case CISP_CMD_CH_AE_START:
        g_isp.ae.enabled = 1;
        break;

    case CISP_CMD_CH_AE_STOP:
        g_isp.ae.enabled = 0;
        break;

    case CISP_CMD_CH_AE_WINDOW_PARAM_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            g_isp_state.ae_metering = (uint32_t)param->value;
            isp_ae_set_metering(g_isp_state.ae_metering);
        }
        break;

    case CISP_CMD_CH_AE_TARGET_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            isp_ae_set_target((uint8_t)param->value);
        }
        break;

    case CISP_CMD_CH_AE_INTEGRATION_TIME_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            isp_ae_set_integration_time((uint32_t)param->value);
        }
        break;

    case CISP_CMD_CH_AE_MANUAL_MODE_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            isp_ae_set_manual_mode((uint32_t)param->value);
        }
        break;

    /* 6. 3A Auto-White-Balance */
    case CISP_CMD_CH_AWB_START:
        g_isp_state.awb_enabled = 1;
        g_isp.awb.enabled = 1;
        break;

    case CISP_CMD_CH_AWB_STOP:
        g_isp_state.awb_enabled = 0;
        g_isp.awb.enabled = 0;
        break;

    case CISP_CMD_CH_AWB_CCT_MANUAL:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            isp_awb_set_cct((uint32_t)param->value);
        }
        break;

    case CISP_CMD_CH_AWB_CCT_GET:
        resp_data = isp_awb_get_cct();
        break;

    case CISP_CMD_CH_AWB_1ST_GAIN_MANUAL:
        if (payload) {
            const struct fthd_cmd_awb_gains *gains = (const struct fthd_cmd_awb_gains *)payload;
            isp_awb_set_manual_gains((uint16_t)gains->r_gain, (uint16_t)gains->b_gain);
        }
        break;

    /* 7. 3A Auto-Focus & AD5816 VCM Actuator */
    case CISP_CMD_CH_AF_START:
    case CISP_CMD_CH_AF_ONE_SHOT:
        isp_af_start_oneshot();
        ipc_terminal_write("AF: Starting CDAF search sweep\n");
        break;

    case CISP_CMD_CH_AF_STOP:
        g_isp.af.state = AF_STATE_IDLE;
        break;

    case CISP_CMD_CH_FOCUS_POSITION_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            isp_actuator_set_position((uint16_t)param->value);
        }
        break;

    case CISP_CMD_CH_FOCUS_POSITION_GET:
        resp_data = isp_actuator_get_position();
        break;

    case CISP_CMD_CH_FOCUS_LIMITS_SET:
        if (payload) {
            const struct fthd_cmd_focus_limits *lim = (const struct fthd_cmd_focus_limits *)payload;
            isp_actuator_set_limits((uint16_t)lim->min, (uint16_t)lim->max);
        }
        break;

    case CISP_CMD_CH_FOCUS_STEP_SIZE_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            isp_actuator_set_step_size((uint16_t)param->value);
        }
        break;

    /* 8. Dynamic Range Compression (DRC/HDR) */
    case CISP_CMD_CH_DRC_START:
        g_isp_state.drc_enabled = 1;
        isp_drc_enable(1);
        ipc_terminal_write("DRC_START: Tone curve adaptation enabled\n");
        break;

    case CISP_CMD_CH_DRC_STOP:
        g_isp_state.drc_enabled = 0;
        isp_drc_enable(0);
        break;

    case CISP_CMD_CH_DRC_SET:
        if (payload) {
            const struct fthd_cmd_int_param *param = (const struct fthd_cmd_int_param *)payload;
            isp_drc_set_strength((uint32_t)param->value);
        }
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

    /* Store response payload if applicable */
    volatile uint32_t *resp_val = (volatile uint32_t *)&g_ringbuffers[6][sizeof(struct fthd_cmd_header)];
    *resp_val = resp_data;

    return 0;
}

void ipc_poll_commands(void)
{
    /* 1. Check IO ringbuffer (Channel 3, H2T) for incoming host command descriptors */
    volatile struct fthd_ringbuf_entry *entries = (volatile struct fthd_ringbuf_entry *)&g_ringbuffers[3][0];
    for (int i = 0; i < RINGBUF_ENTRIES; i++) {
        uint32_t flags = entries[i].address_flags;
        if (flags != 0 && (flags & 1) == 0) {
            uint32_t addr = flags & ~3;
            volatile struct fthd_cmd_header *cmd = (volatile struct fthd_cmd_header *)addr;
            void *payload = (void *)(addr + sizeof(struct fthd_cmd_header));
            ipc_dispatch_command((uint32_t)cmd->opcode, payload, entries[i].request_size);
            cmd->status = 0; /* Success */
            entries[i].address_flags = flags | 1; /* Mark entry complete for host */
            bcm1570_signal_host(0x10 << 3); /* Signal IO IRQ to host */
        }
    }

    /* Direct memory command header fallback (for unit tests / mock harness) */
    volatile struct fthd_cmd_header *hdr = (volatile struct fthd_cmd_header *)&g_ringbuffers[3][0];
    if (hdr->opcode != 0 && (hdr->status == 0xdead || hdr->status == 0)) {
        uint32_t op = hdr->opcode;
        const void *payload = (const void *)&g_ringbuffers[3][sizeof(struct fthd_cmd_header)];
        ipc_dispatch_command(op, payload, 32);
        hdr->opcode = 0;
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
