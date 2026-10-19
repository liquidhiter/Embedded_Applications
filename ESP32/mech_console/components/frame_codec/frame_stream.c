/*
 * @file    frame_stream.c
 * @author  maxin
 * @date    2026-10-19
 * @brief   Incremental zero-delimited frame collection.
 *
 * SPDX-License-Identifier: MIT
 */

#include "frame_stream.h"

#include <stdint.h>

frame_codec_result_t frame_stream_init(frame_stream_t *stream, frame_stream_callback_t callback, void *context)
{
    if (stream == NULL || callback == NULL) {
        return FRAME_CODEC_INVALID_ARG;
    }

    stream->length = 0;
    stream->discarding = false;
    stream->callback = callback;
    stream->context = context;

    return FRAME_CODEC_OK;
}

frame_codec_result_t frame_stream_feed(frame_stream_t *stream, const uint8_t *data, size_t length)
{
    if (stream == NULL || (data == NULL && length != 0)) {
        return FRAME_CODEC_INVALID_ARG;
    }

    for (size_t i = 0; i < length; i++) {
        if (stream->discarding) {
            if (data[i] == 0) {
                stream->discarding = false;
            }
            /* Discarding data until the next frame delimiter (0) */
            continue;
        }

        if (data[i] == 0) {
            /* Check length first to ensure we have a valid frame before invoking the callback
             * Corner case: data[0] == 0 after init
             */
            if (stream->length > 0 && stream->callback != NULL) {
                stream->callback(stream->buffer, stream->length, stream->context);
            }
            stream->length = 0;
        } else {
            if (stream->length < FRAME_STREAM_MAX_ENCODED_SIZE) {
                stream->buffer[stream->length++] = data[i];
            } else {
                stream->discarding = true;
                stream->length = 0;
            }
        }
    }

    return FRAME_CODEC_OK;
}
