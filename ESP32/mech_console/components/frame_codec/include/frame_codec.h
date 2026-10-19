/*
 * @file    frame_codec.h
 * @author  maxin
 * @date    2026-10-18
 * @brief   Frame codec public interface.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
    FRAME_CODEC_OK = 0,
    FRAME_CODEC_INVALID_ARG,
    FRAME_CODEC_NO_SPACE,
    FRAME_CODEC_MALFORMED,
} frame_codec_result_t;

/**
 * @brief Encode a frame using COBS (Consistent Overhead Byte Stuffing).
 *
 * @param src Pointer to the source data to encode.
 * @param src_len Length of the source data.
 * @param dst Pointer to the destination buffer to store the encoded data.
 * @param dst_capacity Capacity of the destination buffer.
 * @param dst_len Pointer to a variable to store the length of the encoded data.
 * @return frame_codec_result_t Result of the encoding operation.
 */
frame_codec_result_t frame_cobs_encode(const uint8_t *src,
                                       size_t src_len,
                                       uint8_t *dst,
                                       size_t dst_capacity,
                                       size_t *dst_len);

/**
 * @brief Decode a frame using COBS (Consistent Overhead Byte Stuffing).
 *
 * @param src Pointer to the source data to decode.
 * @param src_len Length of the source data.
 * @param dst Pointer to the destination buffer to store the decoded data.
 * @param dst_capacity Capacity of the destination buffer.
 * @param dst_len Pointer to a variable to store the length of the decoded data.
 * @return frame_codec_result_t Result of the decoding operation.
 */
frame_codec_result_t frame_cobs_decode(const uint8_t *src,
                                       size_t src_len,
                                       uint8_t *dst,
                                       size_t dst_capacity,
                                       size_t *dst_len);
