/*
 * @file    frame_common.h
 * @author  maxin
 * @date    2026-10-19
 * @brief   Common definitions for the frame codec, including result codes.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/** @brief Result codes for frame codec operations */
typedef enum {
    FRAME_CODEC_OK = 0,         /**< Operation successful */
    FRAME_CODEC_INVALID_ARG,    /**< Invalid argument */
    FRAME_CODEC_NO_SPACE,       /**< Insufficient space in the buffer */
    FRAME_CODEC_MALFORMED,      /**< Malformed frame */
} frame_codec_result_t;
