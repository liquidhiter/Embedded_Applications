#include <string.h>

#include "unity.h"

#include "frame_codec.h"
#include "frame_crc.h"
#include "frame_packet.h"

/* Independent references: Python struct.pack('<BBHQIHH', ...) and zlib.crc32.
 * The final byte in each wire vector is the frame delimiter.
 */
static const uint8_t empty_wire[] = {
    0x03, 0x01, 0x20, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x05, 0xDF, 0xD6, 0x36, 0x4C, 0x00,
};

static const uint8_t golden_wire[] = {
    0x03, 0x01, 0x20, 0x01, 0x0E, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x98, 0xBA,
    0xDC, 0xFE, 0x03, 0x01, 0x01, 0x02, 0x11, 0x06, 0x22, 0xA2, 0xAE, 0xAA, 0x1E, 0x00,
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
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_packet_encode(&packet, wire, sizeof(wire), &length));
    TEST_ASSERT_EQUAL_UINT(sizeof(golden_wire), length);
    TEST_ASSERT_EQUAL_MEMORY(golden_wire, wire, length);
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_decode(wire, length - 1, raw, sizeof(raw), &raw_length));
    const uint8_t expected[] = {
        1,    0x20, 0, 0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x98, 0xBA,
        0xDC, 0xFE, 3, 0, 0,    0,    0x11, 0,    0x22, 0xA2, 0xAE, 0xAA, 0x1E,
    };
    TEST_ASSERT_EQUAL_UINT(sizeof(expected), raw_length);
    TEST_ASSERT_EQUAL_MEMORY(expected, raw, raw_length);
}

TEST_CASE("Packet encode supports an empty payload", "[frame_packet]")
{
    frame_packet_t packet = {.type = 0x20};
    uint8_t wire[sizeof(empty_wire)];
    size_t length = 99;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_packet_encode(&packet, wire, sizeof(wire), &length));
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
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_packet_encode(&packet, wire, sizeof(wire), NULL));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_packet_encode(NULL, wire, sizeof(wire), &length));
    TEST_ASSERT_EQUAL_UINT(0, length);
    length = 99;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_packet_encode(&packet, NULL, sizeof(wire), &length));
    TEST_ASSERT_EQUAL_UINT(0, length);
    const uint16_t invalid_lengths[] = {513, UINT16_MAX};
    for (size_t i = 0; i < 2; ++i) {
        packet.payload_length = invalid_lengths[i];
        length = 99;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_packet_encode(&packet, wire, sizeof(wire), &length));
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
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_packet_encode(&packet, storage + 1, 540, &length));
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
            1, 0x20, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 2, 0, 0,
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
    memset(packet.payload + packet.payload_length, 0xAA, sizeof(packet.payload) - packet.payload_length);
    uint8_t wire[sizeof(golden_wire)];
    size_t length = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_packet_encode(&packet, wire, sizeof(wire), &length));
    TEST_ASSERT_EQUAL_UINT(sizeof(golden_wire), length);
    TEST_ASSERT_EQUAL_MEMORY(golden_wire, wire, length);
}

static void assert_packet_fields(const frame_packet_t *expected, const frame_packet_t *actual)
{
    TEST_ASSERT_EQUAL_HEX8(expected->type, actual->type);
    /* Unity's default ESP-IDF configuration does not enable 64-bit assertions. */
    TEST_ASSERT_EQUAL_HEX32((uint32_t)expected->session_id, (uint32_t)actual->session_id);
    TEST_ASSERT_EQUAL_HEX32((uint32_t)(expected->session_id >> 32), (uint32_t)(actual->session_id >> 32));
    TEST_ASSERT_EQUAL_HEX32(expected->sequence, actual->sequence);
    TEST_ASSERT_EQUAL_UINT16(expected->payload_length, actual->payload_length);
    if (expected->payload_length != 0) {
        TEST_ASSERT_EQUAL_MEMORY(expected->payload, actual->payload, expected->payload_length);
    }
}

static void assert_decode_failure(const uint8_t *encoded, size_t length, frame_codec_result_t expected)
{
    frame_packet_t packet;
    uint8_t original[sizeof(packet)];
    memset(&packet, 0xA5, sizeof(packet));
    memcpy(original, &packet, sizeof(packet));
    TEST_ASSERT_EQUAL_INT(expected, frame_packet_decode(encoded, length, &packet));
    TEST_ASSERT_EQUAL_MEMORY(original, &packet, sizeof(packet));
}

/* Recompute CRC for intentionally invalid headers so CRC cannot mask validation bugs. */
static size_t encode_test_raw(uint8_t *raw, size_t length, uint8_t *encoded, size_t capacity)
{
    uint32_t crc = 0;
    TEST_ASSERT_TRUE(length >= 4);
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_crc32(raw, length - 4, &crc));
    for (size_t i = 0; i < 4; ++i) {
        raw[length - 4 + i] = (uint8_t)(crc >> (8 * i));
    }
    size_t encoded_length = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_encode(raw, length, encoded, capacity, &encoded_length));
    return encoded_length;
}

TEST_CASE("Packet decode matches independent wire references", "[frame_packet]")
{
    frame_packet_t packet;
    frame_packet_t expected = example_packet();
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_packet_decode(golden_wire, sizeof(golden_wire) - 1, &packet));
    assert_packet_fields(&expected, &packet);
    expected = (frame_packet_t){.type = 0x20};
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_packet_decode(empty_wire, sizeof(empty_wire) - 1, &packet));
    assert_packet_fields(&expected, &packet);
}

TEST_CASE("Packet decode rejects invalid arguments without mutation", "[frame_packet]")
{
    assert_decode_failure(NULL, 0, FRAME_CODEC_INVALID_ARG);
    assert_decode_failure(NULL, 10, FRAME_CODEC_INVALID_ARG);
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_packet_decode(golden_wire, sizeof(golden_wire) - 1, NULL));
    assert_decode_failure(golden_wire, 0, FRAME_CODEC_MALFORMED);
}

TEST_CASE("Packet decode rejects malformed COBS and delimiters", "[frame_packet]")
{
    const uint8_t bad_code[] = {0};
    const uint8_t short_block[] = {3, 0x11};
    const uint8_t zero_data[] = {2, 0};
    const uint8_t empty_raw[] = {1};
    assert_decode_failure(bad_code, sizeof(bad_code), FRAME_CODEC_MALFORMED);
    assert_decode_failure(short_block, sizeof(short_block), FRAME_CODEC_MALFORMED);
    assert_decode_failure(zero_data, sizeof(zero_data), FRAME_CODEC_MALFORMED);
    assert_decode_failure(empty_raw, sizeof(empty_raw), FRAME_CODEC_MALFORMED);
    assert_decode_failure(golden_wire, sizeof(golden_wire), FRAME_CODEC_MALFORMED);
}

TEST_CASE("Packet decode rejects inconsistent and oversized lengths", "[frame_packet]")
{
    uint8_t raw[537] = {0};
    uint8_t encoded[541];
    /* Exercise every raw length below the minimum header plus CRC. */
    for (size_t length = 0; length < 24; ++length) {
        size_t encoded_length = 0;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK,
                              frame_cobs_encode(raw, length, encoded, sizeof(encoded), &encoded_length));
        assert_decode_failure(encoded, encoded_length, FRAME_CODEC_MALFORMED);
    }
    /* Header declares 3 bytes; shorter and longer actual packets have valid CRCs. */
    raw[0] = 1;
    raw[16] = 3;
    const size_t lengths[] = {24, 26, 28, 536};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        size_t length = encode_test_raw(raw, lengths[i], encoded, sizeof(encoded));
        assert_decode_failure(encoded, length, FRAME_CODEC_MALFORMED);
    }
    const uint16_t declared[] = {513, UINT16_MAX};
    for (size_t i = 0; i < sizeof(declared) / sizeof(declared[0]); ++i) {
        memset(raw, 0, sizeof(raw));
        raw[0] = 1;
        raw[16] = (uint8_t)declared[i];
        raw[17] = (uint8_t)(declared[i] >> 8);
        size_t length = encode_test_raw(raw, sizeof(raw), encoded, sizeof(encoded));
        assert_decode_failure(encoded, length, FRAME_CODEC_MALFORMED);
        length = encode_test_raw(raw, 24, encoded, sizeof(encoded));
        assert_decode_failure(encoded, length, FRAME_CODEC_MALFORMED);
    }
}

TEST_CASE("Packet decode validates every reserved header byte", "[frame_packet]")
{
    const size_t offsets[] = {0, 2, 3, 18, 19};
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        uint8_t raw[24] = {1};
        uint8_t encoded[26];
        raw[offsets[i]] = offsets[i] == 0 ? 2 : 1;
        size_t length = encode_test_raw(raw, sizeof(raw), encoded, sizeof(encoded));
        assert_decode_failure(encoded, length, FRAME_CODEC_MALFORMED);
    }
}

TEST_CASE("Packet decode rejects corruption across the CRC coverage", "[frame_packet]")
{
    uint8_t original[27];
    size_t raw_length = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_decode(golden_wire, sizeof(golden_wire) - 1, original,
                                                            sizeof(original), &raw_length));
    /* Mutate every bit, including the stored CRC, without recomputing it. */
    for (size_t byte = 0; byte < raw_length; ++byte) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            uint8_t raw[27], encoded[29];
            memcpy(raw, original, sizeof(raw));
            raw[byte] ^= (uint8_t)(1U << bit);
            size_t length = 0;
            TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK,
                                  frame_cobs_encode(raw, sizeof(raw), encoded, sizeof(encoded), &length));
            assert_decode_failure(encoded, length, FRAME_CODEC_MALFORMED);
        }
    }
}

TEST_CASE("Packet round trip covers all payload lengths and preserves input", "[frame_packet]")
{
    frame_packet_t expected = example_packet();
    struct {
        uint32_t before;
        frame_packet_t packet;
        uint32_t after;
    } output;
    uint8_t encoded[540], original[540];
    for (size_t length = 0; length <= 512; ++length) {
        expected.type = (uint8_t)length;
        expected.session_id = length == 0 ? 0 : UINT64_MAX;
        expected.sequence = length == 0 ? 0 : UINT32_MAX;
        expected.payload_length = (uint16_t)length;
        for (size_t i = 0; i < length; ++i) {
            expected.payload[i] = (uint8_t)i;
        }
        size_t encoded_length = 0;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK,
                              frame_packet_encode(&expected, encoded, sizeof(encoded), &encoded_length));
        memcpy(original, encoded, encoded_length);
        memset(&output, 0xA5, sizeof(output));
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_packet_decode(encoded, encoded_length - 1, &output.packet));
        assert_packet_fields(&expected, &output.packet);
        TEST_ASSERT_EQUAL_HEX32(0xA5A5A5A5, output.before);
        TEST_ASSERT_EQUAL_HEX32(0xA5A5A5A5, output.after);
        TEST_ASSERT_EQUAL_MEMORY(original, encoded, encoded_length);
    }
}
