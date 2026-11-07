/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * OpenFTHD: Open-Source Firmware for Broadcom BCM1570 FaceTime HD Camera
 *
 * Compute Engine: ARM Cortex-A7 NEON SIMD Vector Acceleration & AES Crypto Engine
 *
 * Copyright (C) 2026 Carlos Alberto & Contributors
 */

#ifndef _OPENFTHD_COMPUTE_H
#define _OPENFTHD_COMPUTE_H

#include <stdint.h>
#include <stddef.h>

/* Compute Channel Opcodes */
enum fthd_compute_opcodes {
    /* ARM NEON SIMD Vector Acceleration (0x10+) */
    COMPUTE_OP_NEON_YUYV_TO_RGB      = 0x10,
    COMPUTE_OP_NEON_CONVOLUTION_3X3  = 0x11,
    COMPUTE_OP_NEON_DOT_PRODUCT      = 0x12,

    /* Cryptographic Hardware / Coprocessor Offload (0x20+) */
    COMPUTE_OP_AES_128_ENCRYPT       = 0x20,
    COMPUTE_OP_AES_128_DECRYPT       = 0x21,
    COMPUTE_OP_AES_128_CBC_ENCRYPT   = 0x22,
    COMPUTE_OP_AES_128_CBC_DECRYPT   = 0x23,

    /* Benchmark & Diagnostic (0x30+) */
    COMPUTE_OP_BENCHMARK             = 0x30,
};

/* Compute Request / Response Header */
struct __attribute__((packed)) fthd_compute_header {
    uint32_t opcode;        /* enum fthd_compute_opcodes */
    uint32_t flags;         /* status on response */
    uint32_t data_len;      /* payload length in bytes */
    uint32_t sequence;      /* transaction identifier */
};

/* AES Cryptographic Request Payload */
struct __attribute__((packed)) fthd_compute_aes_payload {
    uint8_t key[16];        /* 128-bit key */
    uint8_t iv[16];         /* 128-bit initialization vector (CBC) */
    uint32_t num_blocks;    /* Number of 16-byte blocks to process */
    uint8_t data[64];       /* Input block data (up to 4 blocks per packet) */
};

/* NEON SIMD Matrix / Tensor Dot Product Request */
struct __attribute__((packed)) fthd_compute_neon_dot_payload {
    uint32_t length;        /* vector length (must be multiple of 4) */
    int16_t  vec_a[16];     /* Vector A (int16_t) */
    int16_t  vec_b[16];     /* Vector B (int16_t) */
};

/* NEON SIMD Function Declarations */
void neon_yuyv_to_rgb(const uint8_t *yuyv, uint8_t *rgb, size_t num_pixels);
void neon_sobel_3x3(const uint8_t *input, uint8_t *output, size_t width, size_t height);
int32_t neon_dot_product_s16(const int16_t *a, const int16_t *b, size_t length);

/* AES Engine Function Declarations */
struct aes128_ctx {
    uint32_t round_keys[44];
};

void aes128_key_expansion(struct aes128_ctx *ctx, const uint8_t *key);
void aes128_encrypt_block(const struct aes128_ctx *ctx, const uint8_t *input, uint8_t *output);
void aes128_decrypt_block(const struct aes128_ctx *ctx, const uint8_t *input, uint8_t *output);
void aes128_cbc_encrypt(const struct aes128_ctx *ctx, uint8_t *iv, const uint8_t *input, uint8_t *output, size_t length);
void aes128_cbc_decrypt(const struct aes128_ctx *ctx, uint8_t *iv, const uint8_t *input, uint8_t *output, size_t length);

/* Compute Engine Dispatcher */
int compute_dispatch(const struct fthd_compute_header *hdr, const void *payload, void *resp_buf, uint32_t *resp_len);

#endif /* _OPENFTHD_COMPUTE_H */
