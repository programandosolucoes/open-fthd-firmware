/* SPDX-License-Identifier: GPL-2.0-only */
/**
 * ============================================================================
 * libfthd - Clean-room C API for FaceTime HD (BCM1570 PCIe ISP) Camera Controls
 * ============================================================================
 *
 * This header provides an educational, didactic, and dependency-free C library
 * allowing amateur developers, students, and system applications to control:
 * 1. Hardware ePTZ Digital Zoom & Sensor Pan/Tilt (via V4L2 Selection API)
 * 2. On-chip Auto-Exposure Photometry Metering (Spot, Matrix, Center, Average)
 * 3. Dynamic Range Compression / Wide Dynamic Range (HDR) hardware processing
 * 4. Privacy Indicator LED override (Auto, Force Off, Force On)
 * 5. Co-processor and sensor power saving profiles (High Perf, Balanced, Low Power)
 */

#ifndef LIBFTHD_H
#define LIBFTHD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Auto-Exposure Metering Modes */
typedef enum {
    FTHD_METERING_AVERAGE         = 0, /* Average scene illumination */
    FTHD_METERING_CENTER_WEIGHTED = 1, /* Prioritizes center subject */
    FTHD_METERING_SPOT            = 2, /* Pinpoint spot (faces against bright light) */
    FTHD_METERING_MATRIX          = 3, /* Multi-zone evaluative grid */
} fthd_metering_mode_t;

/* Camera Indicator LED Modes */
typedef enum {
    FTHD_LED_AUTO      = 0, /* Hardware default: on while streaming */
    FTHD_LED_FORCE_OFF = 1, /* Privacy stealth mode: suppress LED */
    FTHD_LED_FORCE_ON  = 2, /* Continuously illuminated */
} fthd_led_mode_t;

/* Energy / Power Saving Profiles */
typedef enum {
    FTHD_POWER_PERFORMANCE = 0, /* Low latency: maintains ISP/DDR clocks active */
    FTHD_POWER_BALANCED    = 1, /* Standard standby clock gating when idle */
    FTHD_POWER_LOW_POWER   = 2, /* Aggressive: deep sensor sleep on stream stop */
} fthd_power_profile_t;

/* Camera device handle */
typedef struct fthd_device fthd_device_t;

/**
 * Open a FaceTime HD camera device node (e.g. "/dev/video0").
 * Returns pointer to handle, or NULL on error.
 */
fthd_device_t *fthd_open(const char *device_path);

/**
 * Close FaceTime HD camera device handle and release file descriptor.
 */
void fthd_close(fthd_device_t *dev);

/**
 * Hardware ePTZ: Set sensor sub-rectangle crop window for lossless zoom.
 * @param left   Left pixel coordinate (must be even for YUV alignment)
 * @param top    Top pixel coordinate (must be even)
 * @param width  Width in pixels
 * @param height Height in pixels
 * @return 0 on success, negative errno on failure.
 */
int fthd_set_crop(fthd_device_t *dev, uint32_t left, uint32_t top, uint32_t width, uint32_t height);

/**
 * Hardware ePTZ: Query current sensor crop window.
 */
int fthd_get_crop(fthd_device_t *dev, uint32_t *left, uint32_t *top, uint32_t *width, uint32_t *height);

/**
 * Hardware ePTZ: Set centered digital zoom factor (e.g., 1.0f = full sensor, 2.0f = 2x zoom).
 */
int fthd_set_zoom(fthd_device_t *dev, float zoom_factor);

/**
 * Reset crop window to full sensor frame.
 */
int fthd_reset_crop(fthd_device_t *dev);

/**
 * Configure on-chip Auto-Exposure Photometry metering mode.
 */
int fthd_set_exposure_metering(fthd_device_t *dev, fthd_metering_mode_t mode);

/**
 * Query current Auto-Exposure metering mode.
 */
int fthd_get_exposure_metering(fthd_device_t *dev, fthd_metering_mode_t *mode);

/**
 * Enable or disable hardware Wide Dynamic Range (HDR / Dynamic Range Compression).
 */
int fthd_set_hdr(fthd_device_t *dev, bool enable);

/**
 * Query hardware HDR / DRC state.
 */
int fthd_get_hdr(fthd_device_t *dev, bool *enabled);

/**
 * Set camera indicator LED mode (Auto, Force Off, Force On).
 */
int fthd_set_led(fthd_device_t *dev, fthd_led_mode_t mode);

/**
 * Query camera indicator LED mode.
 */
int fthd_get_led(fthd_device_t *dev, fthd_led_mode_t *mode);

/**
 * Set kernel driver energy/power profile (via sysfs).
 */
int fthd_set_power_profile(fthd_power_profile_t profile);

/**
 * Query kernel driver energy/power profile (via sysfs).
 */
int fthd_get_power_profile(fthd_power_profile_t *profile);

/**
 * Print a human-readable diagnostic status of all hardware features.
 */
void fthd_print_status(fthd_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* LIBFTHD_H */
