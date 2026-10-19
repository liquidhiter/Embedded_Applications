/*
 * @file    frame_packet.h
 * @author  maxin
 * @date    2026-10-19
 * @brief   Frame packet handling for the frame codec.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "frame_common.h"

/*! @brief Frame packet version. */
#define FRAME_PACKET_VER (1U)

/**
 * @brief   Frame packet structure for the frame codec.
 *
 * This structure represents a frame packet, including its type, session ID, sequence number, and payload.
 */
typedef struct {
    uint8_t type;
    uint64_t session_id;
    uint32_t sequence;

    uint16_t payload_length;
    uint8_t payload[FRAME_PACKET_MAX_PAYLOAD_SIZE];
} frame_packet_t;

/**
 * @brief   Encode a frame packet into a byte array.
 *
 * @param   packet          Pointer to the frame packet to encode.
 * @param   output          Pointer to the output buffer.
 * @param   output_capacity Capacity of the output buffer.
 * @param   output_length   Pointer to the variable to store the length of the encoded output.
 *
 * @return  Result of the encoding operation.
 */
frame_codec_result_t frame_packet_encode(const frame_packet_t *packet, uint8_t *output, size_t output_capacity,
                                         size_t *output_length);

/**
 * @brief   Decode a byte array into a frame packet.
 *
 * @param   encoded         Pointer to the encoded byte array.
 * @param   encoded_length  Length of the encoded byte array.
 * @param   packet          Pointer to the frame packet to store the decoded data.
 *
 * @return  Result of the decoding operation.
 */
frame_codec_result_t frame_packet_decode(const uint8_t *encoded, size_t encoded_length, frame_packet_t *packet);
