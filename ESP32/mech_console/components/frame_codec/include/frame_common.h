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
    FRAME_CODEC_OK = 0,      /**< Operation successful */
    FRAME_CODEC_INVALID_ARG, /**< Invalid argument */
    FRAME_CODEC_NO_SPACE,    /**< Insufficient space in the buffer */
    FRAME_CODEC_MALFORMED,   /**< Malformed frame */
} frame_codec_result_t;

/* Frame Packet Definitions */

/*! @brief Length of the header in a frame packet. */
#define FRAME_PACKET_HEADER_LEN (20U)

/*! @brief Offsets of various fields within the frame packet header. */
#define FRAME_PACKET_HEADER_VERSION_OFFSET (0U)
#define FRAME_PACKET_HEADER_TYPE_OFFSET (1U)
#define FRAME_PACKET_HEADER_FLAGS_OFFSET (2U)
#define FRAME_PACKET_HEADER_SESSION_ID_OFFSET (4U)
#define FRAME_PACKET_HEADER_SEQUENCE_OFFSET (12U)
#define FRAME_PACKET_HEADER_PAYLOAD_LENGTH_OFFSET (16U)
#define FRAME_PACKET_HEADER_RESERVED_OFFSET (18U)

/*! @brief Length of the CRC32 field in a frame packet. */
#define FRAME_PACKET_CRC32_LEN (4U)

/*! @brief Maximum payload size for a frame packet. */
#define FRAME_PACKET_MAX_PAYLOAD_SIZE (512U)

/*! @brief Total length of a frame packet, including header, payload, and CRC32. */
#define FRAME_PACKET_TOTAL_LEN (FRAME_PACKET_HEADER_LEN + FRAME_PACKET_MAX_PAYLOAD_SIZE + FRAME_PACKET_CRC32_LEN)
