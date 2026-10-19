/*
 * @file    frame_packet.c
 * @author  maxin
 * @date    2026-10-19
 * @brief   Frame packet handling for the frame codec.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "frame_codec.h"
#include "frame_crc.h"
#include "frame_packet.h"

/* Helper Functions Prototype */
static void write_u16_le(uint8_t *dst, uint16_t value);
static void write_u32_le(uint8_t *dst, uint32_t value);
static void write_u64_le(uint8_t *dst, uint64_t value);

static uint16_t read_u16_le(const uint8_t *src);
static uint32_t read_u32_le(const uint8_t *src);
static uint64_t read_u64_le(const uint8_t *src);

/* Public API Implementation */
frame_codec_result_t frame_packet_encode(const frame_packet_t *packet, uint8_t *output, size_t output_capacity,
                                         size_t *output_length)
{
    if (output_length == NULL) {
        return FRAME_CODEC_INVALID_ARG;
    }

    *output_length = 0;

    if (packet == NULL || output == NULL) {
        return FRAME_CODEC_INVALID_ARG;
    }

    if (packet->payload_length > FRAME_PACKET_MAX_PAYLOAD_SIZE) {
        return FRAME_CODEC_INVALID_ARG;
    }

    if (output_capacity == 0) {
        return FRAME_CODEC_NO_SPACE;
    }

    uint8_t raw[FRAME_PACKET_TOTAL_LEN];
    raw[FRAME_PACKET_HEADER_VERSION_OFFSET] = FRAME_PACKET_VER;
    raw[FRAME_PACKET_HEADER_TYPE_OFFSET] = packet->type;
    write_u16_le(raw + FRAME_PACKET_HEADER_FLAGS_OFFSET, 0); /* Flags */
    write_u64_le(raw + FRAME_PACKET_HEADER_SESSION_ID_OFFSET, packet->session_id);
    write_u32_le(raw + FRAME_PACKET_HEADER_SEQUENCE_OFFSET, packet->sequence);
    write_u16_le(raw + FRAME_PACKET_HEADER_PAYLOAD_LENGTH_OFFSET, packet->payload_length);
    write_u16_le(raw + FRAME_PACKET_HEADER_RESERVED_OFFSET, 0); /* Reserved */

    /* Copy payload into the raw buffer */
    memcpy(raw + FRAME_PACKET_HEADER_LEN, packet->payload, packet->payload_length);

    frame_codec_result_t result;

    uint32_t crc_val;
    /* Calculate the CRC including the header and payload */
    result = frame_crc32(raw, FRAME_PACKET_HEADER_LEN + packet->payload_length, &crc_val);
    if (result != FRAME_CODEC_OK) {
        return result;
    }

    /* Append the CRC to the raw buffer */
    write_u32_le(raw + FRAME_PACKET_HEADER_LEN + packet->payload_length, crc_val);

    /* Encode the raw buffer into the output buffer */
    size_t encoded_length = 0;
    /* -1 to reserve space for the frame delimiter(0) */
    result = frame_cobs_encode(raw, FRAME_PACKET_HEADER_LEN + packet->payload_length + FRAME_PACKET_CRC32_LEN, output,
                               output_capacity - 1, &encoded_length);
    if (result != FRAME_CODEC_OK) {
        return result;
    }

    /* Append the frame delimiter (0x00) to indicate the end of the COBS-encoded frame */
    output[encoded_length++] = 0x00;
    *output_length = encoded_length;

    return FRAME_CODEC_OK;
}

/* Helper Functions Implementation */

static void write_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
}

static void write_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
    dst[2] = (uint8_t)((value >> 16) & 0xFF);
    dst[3] = (uint8_t)((value >> 24) & 0xFF);
}

static void write_u64_le(uint8_t *dst, uint64_t value)
{
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
    dst[2] = (uint8_t)((value >> 16) & 0xFF);
    dst[3] = (uint8_t)((value >> 24) & 0xFF);
    dst[4] = (uint8_t)((value >> 32) & 0xFF);
    dst[5] = (uint8_t)((value >> 40) & 0xFF);
    dst[6] = (uint8_t)((value >> 48) & 0xFF);
    dst[7] = (uint8_t)((value >> 56) & 0xFF);
}

static uint16_t read_u16_le(const uint8_t *src)
{
    return (uint16_t)(src[0] | (src[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *src)
{
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *src)
{
    return (uint64_t)(src[0] | ((uint64_t)src[1] << 8) | ((uint64_t)src[2] << 16) | ((uint64_t)src[3] << 24) |
                      ((uint64_t)src[4] << 32) | ((uint64_t)src[5] << 40) | ((uint64_t)src[6] << 48) |
                      ((uint64_t)src[7] << 56));
}
