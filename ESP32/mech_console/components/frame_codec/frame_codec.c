/*
 * @file    frame_codec.c
 * @author  maxin
 * @date    2026-10-19
 * @brief   COBS frame encoding and decoding.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "frame_codec.h"
#include "include/frame_codec.h"

frame_codec_result_t frame_cobs_encode(const uint8_t *src,
                                       size_t src_len,
                                       uint8_t *dst,
                                       size_t dst_capacity,
                                       size_t *dst_len)
{
    size_t read_index;
    size_t write_index;
    size_t code_index;
    uint8_t code;

    if (dst_len == NULL) {
        return FRAME_CODEC_INVALID_ARG;
    }

    /* Initialize the output length to default 0 */
    *dst_len = 0;

    if (dst == NULL || (src == NULL && src_len != 0)) {
        return FRAME_CODEC_INVALID_ARG;
    }

    /* +1 for the initial code byte */
    if (dst_capacity == 0) {
        return FRAME_CODEC_NO_SPACE;
    }

    /* Allow empty source buffer */
    if (src == NULL && (src_len == 0)) {
        dst[0] = 1;
        *dst_len = 1;
        return FRAME_CODEC_OK;
    }

    read_index = 0;
    /* Reserve space for the first code byte */
    write_index = 1;
    code_index = 0;
    code = 1;

    while (read_index < src_len) {
        if (src[read_index] == 0) {
            dst[code_index] = code;
            /* Ensure there is space for the next byte */
            if (write_index >= dst_capacity) {
                return FRAME_CODEC_NO_SPACE;
            }
            code_index = write_index++;
            code = 1;
            read_index++;
        } else {
            /* Ensure there is space for the next byte */
            if (write_index >= dst_capacity) {
                return FRAME_CODEC_NO_SPACE;
            }

            dst[write_index++] = src[read_index++];
            code++;

            /* If the code reaches 0xFF, we need to start a new code byte */
            if (code == 0xFF) {
                dst[code_index] = code;
                /* Ensure there is space for the next byte */
                if (write_index >= dst_capacity) {
                    return FRAME_CODEC_NO_SPACE;
                }
                code_index = write_index++;
                code = 1;
            }
        }
    }

    /* Write the final code byte */
    dst[code_index] = code;

    *dst_len = write_index;

    return FRAME_CODEC_OK;
}

frame_codec_result_t frame_cobs_decode(const uint8_t *src,
                                       size_t src_len,
                                       uint8_t *dst,
                                       size_t dst_capacity,
                                       size_t *dst_len)
{
    size_t read_index;
    size_t write_index;
    uint8_t code;

    if (dst_len == NULL) {
        return FRAME_CODEC_INVALID_ARG;
    }

    /* Initialize the output length to default 0 */
    *dst_len = 0;

    /* Source buffer can't be NULL */
    if (dst == NULL || src == NULL || src_len == 0) {
        return FRAME_CODEC_INVALID_ARG;
    }

    read_index = 0;
    write_index = 0;

    while (read_index < src_len) {
        code = src[read_index++];
        /* Validate the **code** byte */
        if (code == 0 || read_index + code - 1 > src_len) {
            /* Malformed COBS frame: code byte is 0 or extends beyond source length */
            return FRAME_CODEC_MALFORMED;
        }
        if (write_index + code - 1 > dst_capacity) {
            return FRAME_CODEC_NO_SPACE;
        }

        if (code > 1) {
            /* Copy the (code - 1) bytes from src to dst */
            for (size_t i = 0; i < code - 1; i++) {
                /* Validate the source byte before copying */
                uint8_t in_byte = src[read_index + i];
                if (in_byte == 0) {
                    /* Zero byte encountered inside data block, which is invalid in COBS */
                    return FRAME_CODEC_MALFORMED;
                }
                dst[write_index + i] = in_byte;
            }
            write_index += code - 1;
            read_index += code - 1;
        }

        /* If the code is less than 0xFF and we haven't reached the end of the source, insert a zero byte */
        if (code < 0xFF && read_index < src_len) {
            if (write_index >= dst_capacity) {
                return FRAME_CODEC_NO_SPACE;
            }
            dst[write_index++] = 0;
        }
    }

    /* Set the final output length */
    *dst_len = write_index;

    return FRAME_CODEC_OK;
}
