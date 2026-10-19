/*
 * @file    frame_stream.h
 * @author  maxin
 * @date    2026-10-19
 * @brief   Stream framer public interface.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "frame_common.h"

/*! @brief Maximum encoded size for a frame in the stream.
 *
 * Header       20 bytes
 * Payload     512 bytes
 * CRC32         4 bytes
 * ─────────────────────
 * Total       536 bytes
 * ─────────────────────
 *
 * COBS: maximum encoded capacity is 536 + 536 / 254 + 1 = 539 bytes
 *
 * Temporary calculation for maximum encoded size, which will be automatically deduced
 * from the actual frame structure later.
 */
#define FRAME_STREAM_MAX_ENCODED_SIZE (539U)

/**
 * @brief Callback function type for handling encoded frames in the stream.
 *
 * @param encoded Pointer to the encoded frame data.
 * @param encoded_len Length of the encoded frame data.
 * @param context User-defined context pointer.
 */
typedef void (*frame_stream_callback_t)(const uint8_t *encoded, size_t encoded_len, void *context);

/**
 * @brief Structure representing a frame stream.
 *
 * This structure holds the buffer for the encoded frame, its length,
 * a flag indicating if the stream is discarding data, and the callback
 * function with its context for handling encoded frames.
 */
typedef struct {
    uint8_t buffer[FRAME_STREAM_MAX_ENCODED_SIZE];
    size_t length;
    bool discarding;

    /* Callback function and its context */
    frame_stream_callback_t callback;
    void *context;
} frame_stream_t;

/**
 * @brief Initialize a frame stream.
 *
 * @param stream Pointer to the frame stream structure to initialize.
 * @param callback Callback function to handle encoded frames.
 * @param context User-defined context pointer for the callback.
 * @return frame_codec_result_t Result of the initialization operation.
 */
frame_codec_result_t frame_stream_init(frame_stream_t *stream, frame_stream_callback_t callback, void *context);

/**
 * @brief Feed data into the frame stream for processing.
 *
 * @param stream Pointer to the frame stream structure.
 * @param data Pointer to the input data to feed into the stream.
 * @param length Length of the input data.
 * @return frame_codec_result_t Result of the feed operation.
 */
frame_codec_result_t frame_stream_feed(frame_stream_t *stream, const uint8_t *data, size_t length);
