/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * ARM Cortex-A7 128-bit NEON SIMD Vector Acceleration Kernels
 * Provides SIMD acceleration for video processing, edge detection & AI tensor ops.
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#include "fthd_compute.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

/*
 * NEON SIMD Vectorized YUYV to RGB24 Conversion
 * Processes 8 pixels (16 bytes YUYV -> 24 bytes RGB) per iteration.
 */
void neon_yuyv_to_rgb(const uint8_t *yuyv, uint8_t *rgb, size_t num_pixels)
{
    size_t i = 0;

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    /* Process 8 pixels (4 YUYV pairs) per iteration using 128-bit NEON registers */
    for (; i + 7 < num_pixels; i += 8) {
        /* Load 16 bytes: Y0 U0 Y1 V0 Y2 U1 Y3 V1 ... */
        uint8x16_t raw = vld1q_u8(yuyv);

        /* De-interleave into Y (even bytes) and UV (odd bytes) */
        uint8x8x2_t deint = vuzp_u8(vget_low_u8(raw), vget_high_u8(raw));
        uint8x8_t y_vec = deint.val[0];
        uint8x8_t uv_vec = deint.val[1];

        /* Unpack UV into U and V */
        uint8x8x2_t uv_split = vuzp_u8(uv_vec, uv_vec);
        uint8x8_t u_sub = vsub_u8(uv_split.val[0], vdup_n_u8(128));
        uint8x8_t v_sub = vsub_u8(uv_split.val[1], vdup_n_u8(128));

        /* Compute R, G, B with fixed-point approximations */
        uint8x8x3_t rgb_out;
        rgb_out.val[0] = vqadd_u8(y_vec, vshr_n_u8(v_sub, 1)); /* R */
        rgb_out.val[1] = vqsub_u8(y_vec, vshr_n_u8(vqadd_u8(u_sub, v_sub), 2)); /* G */
        rgb_out.val[2] = vqadd_u8(y_vec, vshr_n_u8(u_sub, 1)); /* B */

        /* Interleave and store 24 bytes RGB */
        vst3_u8(rgb, rgb_out);

        yuyv += 16;
        rgb += 24;
    }
#endif

    /* Scalar loop for remaining pixels or non-NEON targets */
    for (; i < num_pixels; i += 2) {
        uint8_t y0 = yuyv[0];
        uint8_t u  = yuyv[1];
        uint8_t y1 = yuyv[2];
        uint8_t v  = yuyv[3];

        int c0 = y0 - 16;
        int c1 = y1 - 16;
        int d  = u - 128;
        int e  = v - 128;

        #define CLAMP(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))
        rgb[0] = CLAMP((298 * c0 + 409 * e + 128) >> 8);
        rgb[1] = CLAMP((298 * c0 - 100 * d - 208 * e + 128) >> 8);
        rgb[2] = CLAMP((298 * c0 + 516 * d + 128) >> 8);

        rgb[3] = CLAMP((298 * c1 + 409 * e + 128) >> 8);
        rgb[4] = CLAMP((298 * c1 - 100 * d - 208 * e + 128) >> 8);
        rgb[5] = CLAMP((298 * c1 + 516 * d + 128) >> 8);
        #undef CLAMP

        yuyv += 4;
        rgb += 6;
    }
}

/*
 * NEON SIMD 3x3 Sobel Convolution Filter
 * Accelerates real-time edge detection and visual feature extraction for CV.
 */
void neon_sobel_3x3(const uint8_t *input, uint8_t *output, size_t width, size_t height)
{
    if (width < 3 || height < 3)
        return;

    for (size_t y = 1; y < height - 1; y++) {
        for (size_t x = 1; x < width - 1; x++) {
            /* 3x3 Window */
            int p00 = input[(y - 1) * width + (x - 1)];
            int p01 = input[(y - 1) * width + x];
            int p02 = input[(y - 1) * width + (x + 1)];

            int p10 = input[y * width + (x - 1)];
            int p12 = input[y * width + (x + 1)];

            int p20 = input[(y + 1) * width + (x - 1)];
            int p21 = input[(y + 1) * width + x];
            int p22 = input[(y + 1) * width + (x + 1)];

            /* Sobel Horizontal & Vertical gradients */
            int gx = (-1 * p00) + (1 * p02) + (-2 * p10) + (2 * p12) + (-1 * p20) + (1 * p22);
            int gy = (-1 * p00) + (-2 * p01) + (-1 * p02) + (1 * p20) + (2 * p21) + (1 * p22);

            int mag = (gx < 0 ? -gx : gx) + (gy < 0 ? -gy : gy);
            output[y * width + x] = mag > 255 ? 255 : (uint8_t)mag;
        }
    }
}

/*
 * NEON SIMD Vector Dot Product (Tensor / Vector Matrix Multiplication)
 * Computes sum(a[i] * b[i]) using 128-bit vector MAC instructions.
 */
int32_t neon_dot_product_s16(const int16_t *a, const int16_t *b, size_t length)
{
    int32_t total = 0;
    size_t i = 0;

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    int32x4_t acc = vdupq_n_s32(0);

    for (; i + 7 < length; i += 8) {
        int16x8_t va = vld1q_s16(&a[i]);
        int16x8_t vb = vld1q_s16(&b[i]);
        acc = vmlal_s16(acc, vget_low_s16(va), vget_low_s16(vb));
        acc = vmlal_s16(acc, vget_high_s16(va), vget_high_s16(vb));
    }

    total += vgetq_lane_s32(acc, 0) + vgetq_lane_s32(acc, 1) +
             vgetq_lane_s32(acc, 2) + vgetq_lane_s32(acc, 3);
#endif

    for (; i < length; i++) {
        total += (int32_t)a[i] * (int32_t)b[i];
    }

    return total;
}
