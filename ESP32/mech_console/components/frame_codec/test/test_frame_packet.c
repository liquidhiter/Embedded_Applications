#include <string.h>
#include "frame_codec.h"
#include "frame_packet.h"
#include "unity.h"

/* Independent references: Python struct.pack('<BBHQIHH', ...) and zlib.crc32.
 * The final byte in each wire vector is the frame delimiter.
 */
static const uint8_t empty_wire[] = {
    0x03, 0x01, 0x20, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x05, 0xDF, 0xD6, 0x36, 0x4C, 0x00,
};

static const uint8_t golden_wire[] = {
    0x03, 0x01, 0x20, 0x01, 0x0E, 0x11, 0x22, 0x33, 0x44, 0x55,
    0x66, 0x77, 0x88, 0x98, 0xBA, 0xDC, 0xFE, 0x03, 0x01, 0x01,
    0x02, 0x11, 0x06, 0x22, 0xA2, 0xAE, 0xAA, 0x1E, 0x00,
};

static frame_packet_t example_packet(void)
{
    const frame_packet_t packet = {
        .type = 0x20,
        .session_id = UINT64_C(0x8877665544332211),
        .sequence = UINT32_C(0xFEDCBA98),
        .payload_length = 3,
        .payload = {0x11, 0, 0x22},
    };
    return packet;
}

TEST_CASE("Packet encode matches complete wire and header reference", "[frame_packet]")
{
    frame_packet_t packet = example_packet();
    uint8_t wire[sizeof(golden_wire)], raw[27];
    size_t length = 99, raw_length = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK,
        frame_packet_encode(&packet, wire, sizeof(wire), &length));
    TEST_ASSERT_EQUAL_UINT(sizeof(golden_wire), length);
    TEST_ASSERT_EQUAL_MEMORY(golden_wire, wire, length);
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK,
        frame_cobs_decode(wire, length - 1, raw, sizeof(raw), &raw_length));
    const uint8_t expected[] = {
        1, 0x20, 0, 0,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x98, 0xBA, 0xDC, 0xFE, 3, 0, 0, 0,
        0x11, 0, 0x22, 0xA2, 0xAE, 0xAA, 0x1E,
    };
    TEST_ASSERT_EQUAL_UINT(sizeof(expected), raw_length);
    TEST_ASSERT_EQUAL_MEMORY(expected, raw, raw_length);
}

TEST_CASE("Packet encode supports an empty payload", "[frame_packet]")
{
    frame_packet_t packet = {.type = 0x20};
    uint8_t wire[sizeof(empty_wire)];
    size_t length = 99;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK,
        frame_packet_encode(&packet, wire, sizeof(wire), &length));
    TEST_ASSERT_EQUAL_UINT(sizeof(empty_wire), length);
    TEST_ASSERT_EQUAL_MEMORY(empty_wire, wire, length);
}

TEST_CASE("Packet encode rejects invalid arguments and oversized payloads", "[frame_packet]")
{
    frame_packet_t packet = example_packet();
    uint8_t wire[540], original[540];
    memset(wire, 0xCC, sizeof(wire));
    memcpy(original, wire, sizeof(wire));
    size_t length = 99;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG,
        frame_packet_encode(&packet, wire, sizeof(wire), NULL));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG,
        frame_packet_encode(NULL, wire, sizeof(wire), &length));
    TEST_ASSERT_EQUAL_UINT(0, length);
    length = 99;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG,
        frame_packet_encode(&packet, NULL, sizeof(wire), &length));
    TEST_ASSERT_EQUAL_UINT(0, length);
    const uint16_t invalid_lengths[] = {513, UINT16_MAX};
    for (size_t i = 0; i < 2; ++i) {
        packet.payload_length = invalid_lengths[i];
        length = 99;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG,
            frame_packet_encode(&packet, wire, sizeof(wire), &length));
        TEST_ASSERT_EQUAL_UINT(0, length);
    }
    TEST_ASSERT_EQUAL_MEMORY(original, wire, sizeof(wire));
}

TEST_CASE("Packet encode reserves delimiter capacity and preserves guards", "[frame_packet]")
{
    frame_packet_t packet = example_packet();
    uint8_t storage[sizeof(golden_wire) + 3];
    for (size_t capacity = 0; capacity <= sizeof(golden_wire) + 1; ++capacity) {
        memset(storage, 0xCC, sizeof(storage));
        size_t length = 99;
        frame_codec_result_t result = frame_packet_encode(&packet, storage + 1, capacity, &length);
        TEST_ASSERT_EQUAL_HEX8(0xCC, storage[0]);
        for (size_t i = capacity + 1; i < sizeof(storage); ++i) {
            TEST_ASSERT_EQUAL_HEX8(0xCC, storage[i]);
        }
        if (capacity < sizeof(golden_wire)) {
            TEST_ASSERT_EQUAL_INT(FRAME_CODEC_NO_SPACE, result);
            TEST_ASSERT_EQUAL_UINT(0, length);
        } else {
            TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, result);
            TEST_ASSERT_EQUAL_UINT(sizeof(golden_wire), length);
            TEST_ASSERT_EQUAL_MEMORY(golden_wire, storage + 1, length);
        }
    }
}

TEST_CASE("Packet encode handles maximum payload patterns without mutation", "[frame_packet]")
{
    const uint8_t expected_crc[][4] = {
        {0xE9, 0xA0, 0x23, 0x4C},
        {0x0E, 0x16, 0xF2, 0x43},
        {0xE7, 0xE0, 0xE8, 0xE2},
    };
    const size_t expected_wire_lengths[] = {538, 540, 540};
    for (unsigned pattern = 0; pattern < 3; ++pattern) {
        frame_packet_t packet;
        memset(&packet, 0, sizeof(packet));
        packet.type = 0x20;
        packet.session_id = UINT64_MAX;
        packet.sequence = UINT32_MAX;
        packet.payload_length = 512;
        for (size_t i = 0; i < 512; ++i) {
            packet.payload[i] = pattern == 0 ? 0 : pattern == 1 ? 0xFF : (uint8_t)i;
        }
        frame_packet_t original;
        memcpy(&original, &packet, sizeof(packet));
        uint8_t storage[542], raw[536];
        memset(storage, 0xCC, sizeof(storage));
        size_t length = 0, raw_length = 0;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK,
            frame_packet_encode(&packet, storage + 1, 540, &length));
        TEST_ASSERT_EQUAL_UINT(expected_wire_lengths[pattern], length);
        TEST_ASSERT_EQUAL_HEX8(0xCC, storage[0]);
        TEST_ASSERT_EQUAL_HEX8(0xCC, storage[541]);
        TEST_ASSERT_EQUAL_HEX8(0, storage[length]);
        for (size_t i = 1; i < length; ++i) {
            TEST_ASSERT_TRUE(storage[i] != 0);
        }
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK,
            frame_cobs_decode(storage + 1, length - 1, raw, sizeof(raw), &raw_length));
        TEST_ASSERT_EQUAL_UINT(536, raw_length);
        const uint8_t header[] = {
            1, 0x20, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0, 2, 0, 0,
        };
        TEST_ASSERT_EQUAL_MEMORY(header, raw, sizeof(header));
        TEST_ASSERT_EQUAL_MEMORY(packet.payload, raw + 20, 512);
        TEST_ASSERT_EQUAL_MEMORY(expected_crc[pattern], raw + 532, 4);
        TEST_ASSERT_EQUAL_MEMORY(&original, &packet, sizeof(packet));
        /* One byte less than the actual wire length must fail, including the delimiter. */
        memset(storage, 0xCC, sizeof(storage));
        size_t failed_length = 99;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_NO_SPACE,
            frame_packet_encode(&packet, storage + 1, length - 1, &failed_length));
        TEST_ASSERT_EQUAL_UINT(0, failed_length);
        TEST_ASSERT_EQUAL_HEX8(0xCC, storage[length]);
    }
}

TEST_CASE("Packet encode ignores undeclared payload bytes", "[frame_packet]")
{
    frame_packet_t packet = example_packet();
    memset(packet.payload + packet.payload_length, 0xAA,
           sizeof(packet.payload) - packet.payload_length);
    uint8_t wire[sizeof(golden_wire)];
    size_t length = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK,
        frame_packet_encode(&packet, wire, sizeof(wire), &length));
    TEST_ASSERT_EQUAL_UINT(sizeof(golden_wire), length);
    TEST_ASSERT_EQUAL_MEMORY(golden_wire, wire, length);
}
