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
 * This is an in-memory representation, not a packed wire structure. All multibyte
 * wire fields use little-endian byte order. Offsets below refer to the raw packet
 * after COBS decoding, not to positions in the COBS-encoded input.
 *
 * @verbatim
 * Raw packet (L = payload_length, 0 <= L <= 512):
 *
 * +-------------------------+------------------+------------------+
 * | Header (20 bytes)       | Payload (L bytes)| CRC32 (4 bytes)  |
 * +-------------------------+------------------+------------------+
 * 0                        20                 20+L               24+L
 * |<----------- CRC covers Header + Payload -->|
 *
 * Header byte offsets (inclusive):
 * +---------+----------------+-------+-----------------------------+
 * | Offset  | Field          | Bytes | Value / meaning             |
 * +---------+----------------+-------+-----------------------------+
 * | 0       | version        | 1     | FRAME_PACKET_VER            |
 * | 1       | type           | 1     | Application message type    |
 * | 2..3    | flags          | 2     | Must be zero                |
 * | 4..11   | session_id     | 8     | Sender-provided session ID  |
 * | 12..15  | sequence       | 4     | Sender-provided sequence    |
 * | 16..17  | payload_length | 2     | L                           |
 * | 18..19  | reserved       | 2     | Must be zero                |
 * +---------+----------------+-------+-----------------------------+
 *
 * Wire frame:
 * +------------------------------------------+-------------------+
 * | COBS(Header + Payload + CRC32)            | Delimiter (0x00)  |
 * +------------------------------------------+-------------------+
 * |<--------- frame_packet_decode input ----->|
 * |<-------------- frame_packet_encode output ------------------>|
 * @endverbatim
 *
 * CRC-32/ISO-HDLC covers the raw header and actual payload, excluding the CRC
 * field itself, COBS overhead, and delimiter. The CRC is stored little-endian.
 * The codec preserves session_id and sequence; it does not allocate or advance
 * them. Only the first payload_length bytes of payload are meaningful.
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
 * Serializes the header and payload, appends the CRC, applies COBS, and adds a
 * trailing 0x00 delimiter. Flags and reserved fields are encoded as zero.
 * Buffers are caller-owned; no dynamic allocation is performed. The packet and
 * output buffer must not overlap, and output_length must not alias either.
 *
 * @param   packet          Pointer to the frame packet to encode.
 * @param   output          Pointer to the output buffer.
 * @param   output_capacity Capacity of the output buffer, including delimiter space.
 * @param   output_length   Required output length pointer; includes the delimiter on success.
 *
 * @return  FRAME_CODEC_OK on success, FRAME_CODEC_INVALID_ARG for a NULL required
 *          pointer or payload_length above the maximum, or FRAME_CODEC_NO_SPACE
 *          when the output buffer is too small.
 * @note    On failure, a valid output_length is set to zero. The output buffer
 *          may have been partially written and must not be used.
 */
frame_codec_result_t frame_packet_encode(const frame_packet_t *packet, uint8_t *output, size_t output_capacity,
                                         size_t *output_length);

/**
 * @brief   Decode a byte array into a frame packet.
 *
 * Accepts exactly one COBS-encoded packet WITHOUT the trailing 0x00 delimiter,
 * as provided by the stream framer callback. When decoding encoder output
 * directly, pass output_length - 1 to omit the delimiter.
 *
 * Validates COBS, raw packet length, payload length, version, zero flags and
 * reserved fields, and CRC before writing any result. Type values are preserved
 * without application-level validation. No dynamic allocation is performed.
 * Input and output storage must not overlap.
 *
 * @param   encoded         Required pointer to the encoded packet, excluding delimiter.
 * @param   encoded_length  Number of encoded bytes, excluding delimiter.
 * @param   packet          Required pointer to caller-owned output storage.
 *
 * @return  FRAME_CODEC_OK on success, FRAME_CODEC_INVALID_ARG for a NULL required
 *          pointer, or FRAME_CODEC_MALFORMED for empty input, invalid COBS,
 *          oversized decoded data, invalid header/length fields, or CRC mismatch.
 * @note    On failure, packet remains unchanged. On success, payload bytes beyond
 *          payload_length are unspecified and must not be consumed.
 */
frame_codec_result_t frame_packet_decode(const uint8_t *encoded, size_t encoded_length, frame_packet_t *packet);
