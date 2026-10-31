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

#include "app_utils.h"
#include "frame_codec.h"
#include "frame_crc.h"
#include "frame_packet.h"

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
    app_write_u16_le(raw + FRAME_PACKET_HEADER_FLAGS_OFFSET, 0); /* Flags */
    app_write_u64_le(raw + FRAME_PACKET_HEADER_SESSION_ID_OFFSET, packet->session_id);
    app_write_u32_le(raw + FRAME_PACKET_HEADER_SEQUENCE_OFFSET, packet->sequence);
    app_write_u16_le(raw + FRAME_PACKET_HEADER_PAYLOAD_LENGTH_OFFSET, packet->payload_length);
    app_write_u16_le(raw + FRAME_PACKET_HEADER_RESERVED_OFFSET, 0); /* Reserved */

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
    app_write_u32_le(raw + FRAME_PACKET_HEADER_LEN + packet->payload_length, crc_val);

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

frame_codec_result_t frame_packet_decode(const uint8_t *encoded, size_t encoded_length, frame_packet_t *packet)
{
    if (encoded == NULL || packet == NULL) {
        return FRAME_CODEC_INVALID_ARG;
    }

    /* If encoded pointer is not NULL but length is zero, it's malformed */
    if (encoded != NULL && encoded_length == 0) {
        return FRAME_CODEC_MALFORMED;
    }

    /* Local buffer for the decoded raw frame: only commit to the packet if decoding is successful */
    uint8_t raw[FRAME_PACKET_TOTAL_LEN];
    size_t raw_length = 0;
    frame_codec_result_t result;

    /* Decode the COBS-encoded frame into the raw buffer */
    result = frame_cobs_decode(encoded, encoded_length, raw, sizeof(raw), &raw_length);
    if (result != FRAME_CODEC_OK) {
        return FRAME_CODEC_MALFORMED;
    }

    /* Read the header only when the decoded raw buffer has at least 20 + 4 bytes (header + CRC) */
    if (raw_length < FRAME_PACKET_HEADER_LEN + FRAME_PACKET_CRC32_LEN) {
        /* frame_packet_decode is always called with a COBS-encoded complete frame,
         * so having less than the minimum required length indicates a malformed frame
         */
        return FRAME_CODEC_MALFORMED;
    }

    /* At this point, we have a raw buffer with at least the header and CRC.
     * We can safely read the header fields and validate the CRC.
     */
    uint32_t payload_length = app_read_u16_le(raw + FRAME_PACKET_HEADER_PAYLOAD_LENGTH_OFFSET);
    /* Payload length sanity check */
    if (payload_length > FRAME_PACKET_MAX_PAYLOAD_SIZE) {
        return FRAME_CODEC_MALFORMED;
    }

    /* Validate that the raw buffer contains the full payload and CRC */
    if (raw_length != FRAME_PACKET_HEADER_LEN + payload_length + FRAME_PACKET_CRC32_LEN) {
        return FRAME_CODEC_MALFORMED;
    }

    /* Validate the header fields: version, flags and reserved */
    uint8_t version = raw[FRAME_PACKET_HEADER_VERSION_OFFSET];
    uint16_t flags = app_read_u16_le(raw + FRAME_PACKET_HEADER_FLAGS_OFFSET);
    uint16_t reserved = app_read_u16_le(raw + FRAME_PACKET_HEADER_RESERVED_OFFSET);
    if (version != FRAME_PACKET_VER || flags != 0 || reserved != 0) {
        return FRAME_CODEC_MALFORMED;
    }

    /** @TODO: Check the type field later */

    /* Validate the CRC32 of the packet */
    uint32_t expected_crc = app_read_u32_le(raw + FRAME_PACKET_HEADER_LEN + payload_length);
    uint32_t actual_crc;
    result = frame_crc32(raw, FRAME_PACKET_HEADER_LEN + payload_length, &actual_crc);
    if (result != FRAME_CODEC_OK) {
        return result;
    }

    if (expected_crc != actual_crc) {
        return FRAME_CODEC_MALFORMED;
    }

    /* Commit the decoded frame to the packet */
    packet->type = raw[FRAME_PACKET_HEADER_TYPE_OFFSET];
    packet->session_id = app_read_u64_le(raw + FRAME_PACKET_HEADER_SESSION_ID_OFFSET);
    packet->sequence = app_read_u32_le(raw + FRAME_PACKET_HEADER_SEQUENCE_OFFSET);
    packet->payload_length = payload_length;
    memcpy(packet->payload, raw + FRAME_PACKET_HEADER_LEN, payload_length);

    return FRAME_CODEC_OK;
}
