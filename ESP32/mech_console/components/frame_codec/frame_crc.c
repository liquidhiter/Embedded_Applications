/*
 * @file    frame_crc.c
 * @author  maxin
 * @date    2026-10-19
 * @brief   CRC32 calculation for frames.
 *
 * SPDX-License-Identifier: MIT
 */

#include "frame_crc.h"

#include <stddef.h>
#include <stdint.h>

frame_codec_result_t frame_crc32(const uint8_t *data, size_t length, uint32_t *crc)
{
    if (crc == NULL) {
        return FRAME_CODEC_INVALID_ARG;
    }

    /* Clear the CRC value initially */
    *crc = 0;

    if (data == NULL && length > 0) {
        return FRAME_CODEC_INVALID_ARG;
    }

    if (data == NULL && length == 0) {
        return FRAME_CODEC_OK;
    }

    /* Implement the CRC-32/ISO-HDLC algorithm */
    uint32_t crc_val = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc_val ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc_val & 1) {
                crc_val = (crc_val >> 1) ^ 0xEDB88320;
            } else {
                crc_val >>= 1;
            }
        }
    }
    *crc = crc_val ^ 0xFFFFFFFF;
    return FRAME_CODEC_OK;
}
