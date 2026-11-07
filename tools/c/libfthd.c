/* SPDX-License-Identifier: GPL-2.0-only */
/**
 * ============================================================================
 * libfthd - Clean-room C API Implementation for FaceTime HD ISP Controls
 * ============================================================================
 */

#include "libfthd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#define SYSFS_POWER_SAVE_PATH "/sys/module/facetimehd/parameters/power_save"
#define SYSFS_LED_MODE_PATH   "/sys/module/facetimehd/parameters/led_mode"

struct fthd_device {
    int fd;
    char path[128];
};

fthd_device_t *fthd_open(const char *device_path)
{
    if (!device_path) {
        device_path = "/dev/video0";
    }

    int fd = open(device_path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "[libfthd] Error opening device %s: %s\n", device_path, strerror(errno));
        return NULL;
    }

    fthd_device_t *dev = (fthd_device_t *)calloc(1, sizeof(fthd_device_t));
    if (!dev) {
        close(fd);
        return NULL;
    }

    dev->fd = fd;
    strncpy(dev->path, device_path, sizeof(dev->path) - 1);
    return dev;
}

void fthd_close(fthd_device_t *dev)
{
    if (dev) {
        if (dev->fd >= 0) {
            close(dev->fd);
        }
        free(dev);
    }
}

static int set_v4l2_control(fthd_device_t *dev, uint32_t id, int32_t value)
{
    if (!dev || dev->fd < 0)
        return -EINVAL;

    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = id;
    ctrl.value = value;

    if (ioctl(dev->fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        return -errno;
    }
    return 0;
}

static int get_v4l2_control(fthd_device_t *dev, uint32_t id, int32_t *value)
{
    if (!dev || dev->fd < 0 || !value)
        return -EINVAL;

    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = id;

    if (ioctl(dev->fd, VIDIOC_G_CTRL, &ctrl) < 0) {
        return -errno;
    }
    *value = ctrl.value;
    return 0;
}

int fthd_set_crop(fthd_device_t *dev, uint32_t left, uint32_t top, uint32_t width, uint32_t height)
{
    if (!dev || dev->fd < 0)
        return -EINVAL;

    /*
     * V4L2 Selection API:
     * We submit a target crop rectangle directly to the kernel driver.
     * The Broadcom ISP coprocessor will intercept this and reconfigure
     * its internal hardware scaler/cropper pipeline without host CPU overhead.
     */
    struct v4l2_selection sel;
    memset(&sel, 0, sizeof(sel));
    sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sel.target = V4L2_SEL_TGT_CROP;
    sel.flags = 0;
    sel.r.left = (int32_t)left;
    sel.r.top = (int32_t)top;
    sel.r.width = (int32_t)width;
    sel.r.height = (int32_t)height;

    if (ioctl(dev->fd, VIDIOC_S_SELECTION, &sel) < 0) {
        return -errno;
    }
    return 0;
}

int fthd_get_crop(fthd_device_t *dev, uint32_t *left, uint32_t *top, uint32_t *width, uint32_t *height)
{
    if (!dev || dev->fd < 0)
        return -EINVAL;

    struct v4l2_selection sel;
    memset(&sel, 0, sizeof(sel));
    sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sel.target = V4L2_SEL_TGT_CROP;

    if (ioctl(dev->fd, VIDIOC_G_SELECTION, &sel) < 0) {
        return -errno;
    }

    if (left)   *left = sel.r.left;
    if (top)    *top = sel.r.top;
    if (width)  *width = sel.r.width;
    if (height) *height = sel.r.height;
    return 0;
}

int fthd_set_zoom(fthd_device_t *dev, float zoom_factor)
{
    if (zoom_factor < 1.0f)
        zoom_factor = 1.0f;

    const uint32_t base_w = 1280;
    const uint32_t base_h = 720;

    uint32_t new_w = (uint32_t)((float)base_w / zoom_factor);
    uint32_t new_h = (uint32_t)((float)base_h / zoom_factor);

    /* Hardware requirement: Align to 8 pixels for YUYV macroblocks */
    new_w = (new_w / 8) * 8;
    new_h = (new_h / 8) * 8;

    uint32_t left = (base_w - new_w) / 2;
    uint32_t top = (base_h - new_h) / 2;

    return fthd_set_crop(dev, left, top, new_w, new_h);
}

int fthd_reset_crop(fthd_device_t *dev)
{
    return fthd_set_crop(dev, 0, 0, 1280, 720);
}

int fthd_set_exposure_metering(fthd_device_t *dev, fthd_metering_mode_t mode)
{
    return set_v4l2_control(dev, V4L2_CID_EXPOSURE_METERING, (int32_t)mode);
}

int fthd_get_exposure_metering(fthd_device_t *dev, fthd_metering_mode_t *mode)
{
    int32_t val;
    int ret = get_v4l2_control(dev, V4L2_CID_EXPOSURE_METERING, &val);
    if (ret == 0 && mode) {
        *mode = (fthd_metering_mode_t)val;
    }
    return ret;
}

int fthd_set_hdr(fthd_device_t *dev, bool enable)
{
    return set_v4l2_control(dev, V4L2_CID_WIDE_DYNAMIC_RANGE, enable ? 1 : 0);
}

int fthd_get_hdr(fthd_device_t *dev, bool *enabled)
{
    int32_t val;
    int ret = get_v4l2_control(dev, V4L2_CID_WIDE_DYNAMIC_RANGE, &val);
    if (ret == 0 && enabled) {
        *enabled = (val != 0);
    }
    return ret;
}

int fthd_set_led(fthd_device_t *dev, fthd_led_mode_t mode)
{
    int ret = set_v4l2_control(dev, V4L2_CID_ILLUMINATORS_1, (int32_t)mode);
    if (ret != 0) {
        /* Fallback: try writing sysfs parameter */
        FILE *f = fopen(SYSFS_LED_MODE_PATH, "w");
        if (f) {
            fprintf(f, "%d\n", (int)mode);
            fclose(f);
            return 0;
        }
    }
    return ret;
}

int fthd_get_led(fthd_device_t *dev, fthd_led_mode_t *mode)
{
    int32_t val = 0;
    int ret = get_v4l2_control(dev, V4L2_CID_ILLUMINATORS_1, &val);
    if (ret != 0) {
        FILE *f = fopen(SYSFS_LED_MODE_PATH, "r");
        if (f) {
            if (fscanf(f, "%d", &val) == 1) {
                ret = 0;
            }
            fclose(f);
        }
    }
    if (ret == 0 && mode) {
        *mode = (fthd_led_mode_t)val;
    }
    return ret;
}

int fthd_set_power_profile(fthd_power_profile_t profile)
{
    FILE *f = fopen(SYSFS_POWER_SAVE_PATH, "w");
    if (!f) {
        return -errno;
    }
    fprintf(f, "%d\n", (int)profile);
    fclose(f);
    return 0;
}

int fthd_get_power_profile(fthd_power_profile_t *profile)
{
    if (!profile)
        return -EINVAL;
    FILE *f = fopen(SYSFS_POWER_SAVE_PATH, "r");
    if (!f) {
        return -errno;
    }
    int val = 1;
    if (fscanf(f, "%d", &val) != 1) {
        fclose(f);
        return -EIO;
    }
    fclose(f);
    *profile = (fthd_power_profile_t)val;
    return 0;
}

void fthd_print_status(fthd_device_t *dev)
{
    printf("=====================================================\n");
    printf(" FaceTime HD Camera (BCM1570 PCIe ISP) Hardware Info\n");
    printf("=====================================================\n");
    printf(" Device Node: %s\n", dev ? dev->path : "/dev/video0");

    bool hdr = false;
    if (fthd_get_hdr(dev, &hdr) == 0) {
        printf(" Hardware HDR / DRC:       %s\n", hdr ? "ENABLED" : "DISABLED");
    }

    fthd_metering_mode_t met;
    if (fthd_get_exposure_metering(dev, &met) == 0) {
        const char *names[] = {"Average", "Center-Weighted", "Spot", "Matrix"};
        printf(" Exposure Metering:        %s\n", (met <= 3) ? names[met] : "Custom");
    }

    fthd_led_mode_t led;
    if (fthd_get_led(dev, &led) == 0) {
        const char *led_names[] = {"Auto (Streaming)", "Always Off (Stealth)", "Always On"};
        printf(" Indicator LED Mode:       %s\n", (led <= 2) ? led_names[led] : "Unknown");
    }

    fthd_power_profile_t pwr;
    if (fthd_get_power_profile(&pwr) == 0) {
        const char *pwr_names[] = {"High Performance", "Balanced (Default)", "Aggressive Low Power"};
        printf(" Power Saving Profile:     %s\n", (pwr <= 2) ? pwr_names[pwr] : "Unknown");
    }

    uint32_t l, t, w, h;
    if (fthd_get_crop(dev, &l, &t, &w, &h) == 0) {
        printf(" ePTZ Sensor Crop Window:  left=%u, top=%u, size=%ux%u\n", l, t, w, h);
    }
    printf("=====================================================\n");
}

int32_t fthd_compute_neon_dot_sim(const int16_t *a, const int16_t *b, uint32_t length)
{
    int32_t acc = 0;
    for (uint32_t i = 0; i < length; i++) {
        acc += (int32_t)a[i] * (int32_t)b[i];
    }
    return acc;
}

int fthd_compute_build_aes_packet(uint8_t *buf, size_t max_len, const uint8_t *key, const uint8_t *in, size_t in_len)
{
    size_t total_len = sizeof(struct fthd_tool_compute_header) + sizeof(struct fthd_tool_compute_aes_payload);
    if (!buf || max_len < total_len) {
        return -EINVAL;
    }
    memset(buf, 0, total_len);

    struct fthd_tool_compute_header *hdr = (struct fthd_tool_compute_header *)buf;
    struct fthd_tool_compute_aes_payload *payload = (struct fthd_tool_compute_aes_payload *)(buf + sizeof(*hdr));

    hdr->opcode = FTHD_TOOL_COMPUTE_OP_AES_128_ENCRYPT;
    hdr->flags = 0;
    hdr->data_len = sizeof(*payload);
    hdr->sequence = 1;

    if (key) {
        memcpy(payload->key, key, 16);
    }
    uint32_t blocks = (uint32_t)((in_len + 15) / 16);
    if (blocks > 4) blocks = 4;
    payload->num_blocks = blocks;

    if (in && in_len > 0) {
        size_t cpy = in_len > sizeof(payload->data) ? sizeof(payload->data) : in_len;
        memcpy(payload->data, in, cpy);
    }
    return (int)total_len;
}

int fthd_compute_build_neon_packet(uint8_t *buf, size_t max_len, const int16_t *a, const int16_t *b, uint32_t length)
{
    size_t total_len = sizeof(struct fthd_tool_compute_header) + sizeof(struct fthd_tool_compute_neon_dot_payload);
    if (!buf || max_len < total_len) {
        return -EINVAL;
    }
    memset(buf, 0, total_len);

    struct fthd_tool_compute_header *hdr = (struct fthd_tool_compute_header *)buf;
    struct fthd_tool_compute_neon_dot_payload *payload = (struct fthd_tool_compute_neon_dot_payload *)(buf + sizeof(*hdr));

    hdr->opcode = FTHD_TOOL_COMPUTE_OP_NEON_DOT_PRODUCT;
    hdr->flags = 0;
    hdr->data_len = sizeof(*payload);
    hdr->sequence = 1;

    uint32_t len = length > 16 ? 16 : length;
    payload->length = len;
    if (a) memcpy(payload->vec_a, a, len * sizeof(int16_t));
    if (b) memcpy(payload->vec_b, b, len * sizeof(int16_t));

    return (int)total_len;
}

