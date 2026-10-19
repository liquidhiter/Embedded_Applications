#include <string.h>

#include "unity.h"

#include "frame_codec.h"

#define CODEC_CASE(function, description) TEST_CASE(description, "[frame_codec]")

CODEC_CASE(test_decode_vectors, "COBS decode matches known vectors")
{
    static const struct {
        uint8_t encoded[5];
        size_t encoded_len;
        uint8_t expected[4];
        size_t expected_len;
    } vectors[] = {
        {{1}, 1, {0}, 0},
        {{1, 1}, 2, {0}, 1},
        {{3, 0x11, 0x22}, 3, {0x11, 0x22}, 2},
        {{2, 0x11, 2, 0x22}, 4, {0x11, 0, 0x22}, 3},
        {{1, 2, 0x11}, 3, {0, 0x11}, 2},
        {{2, 0x11, 1}, 3, {0x11, 0}, 2},
        {{1, 1, 1}, 3, {0, 0}, 2},
        {{4, 1, 3, 0xFF}, 4, {1, 3, 0xFF}, 3},
    };
    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
        uint8_t output[4];
        size_t length = 99;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_decode(vectors[i].encoded, vectors[i].encoded_len, output,
                                                                vectors[i].expected_len, &length));
        TEST_ASSERT_EQUAL_UINT(vectors[i].expected_len, length);
        if (length) {
            TEST_ASSERT_EQUAL_MEMORY(vectors[i].expected, output, length);
        }
    }
}

CODEC_CASE(test_decode_arguments, "COBS decode rejects invalid arguments")
{
    const uint8_t input[] = {1};
    uint8_t output = 0xCC;
    size_t length = 99;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_cobs_decode(input, 1, &output, 1, NULL));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_cobs_decode(NULL, 1, &output, 1, &length));
    TEST_ASSERT_EQUAL_UINT(0, length);
    length = 99;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_cobs_decode(input, 1, NULL, 1, &length));
    TEST_ASSERT_EQUAL_UINT(0, length);
    length = 99;
    /* Empty encoded input is invalid; {1} encodes an empty payload. */
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_cobs_decode(input, 0, &output, 1, &length));
    TEST_ASSERT_EQUAL_UINT(0, length);
    TEST_ASSERT_EQUAL_HEX8(0xCC, output);
}

CODEC_CASE(test_decode_truncated, "COBS decode rejects zero codes and truncated blocks")
{
    static const struct {
        uint8_t bytes[4];
        size_t length;
    } cases[] = {
        {{0}, 1}, {{2}, 1}, {{3, 0x11}, 2}, {{0xFF, 0xAA}, 2}, {{2, 0xAA, 0}, 3}, {{2, 0xAA, 3, 0xBB}, 4},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint8_t output[260];
        size_t length = 99;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_MALFORMED,
                              frame_cobs_decode(cases[i].bytes, cases[i].length, output, sizeof(output), &length));
        TEST_ASSERT_EQUAL_UINT(0, length);
    }
}

CODEC_CASE(test_decode_zero_data, "COBS decode rejects zero bytes inside data blocks")
{
    /* A delimiter must never appear inside the encoded input, even as data. */
    const uint8_t cases[][3] = {{2, 0, 0}, {3, 0x11, 0}, {3, 0, 0x22}};
    const size_t lengths[] = {2, 3, 3};
    for (size_t i = 0; i < 3; ++i) {
        uint8_t output[4];
        size_t length = 99;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_MALFORMED,
                              frame_cobs_decode(cases[i], lengths[i], output, sizeof(output), &length));
        TEST_ASSERT_EQUAL_UINT(0, length);
    }
}

CODEC_CASE(test_decode_full_blocks, "COBS decode handles FF blocks without inserting zero")
{
    uint8_t input[511], output[509];
    memset(input, 0xAA, sizeof(input));
    input[0] = 0xFF;
    input[255] = 0xFF;
    input[510] = 1;
    const size_t input_lengths[] = {255, 510, 511};
    const size_t output_lengths[] = {254, 508, 508};
    for (size_t i = 0; i < 3; ++i) {
        size_t length = 99;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK,
                              frame_cobs_decode(input, input_lengths[i], output, output_lengths[i], &length));
        TEST_ASSERT_EQUAL_UINT(output_lengths[i], length);
        for (size_t j = 0; j < length; ++j) {
            TEST_ASSERT_EQUAL_HEX8(0xAA, output[j]);
        }
    }
    input[255] = 1;
    input[256] = 1;
    size_t length = 99;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_decode(input, 257, output, 255, &length));
    TEST_ASSERT_EQUAL_UINT(255, length);
    TEST_ASSERT_EQUAL_HEX8(0, output[254]);
}

CODEC_CASE(test_decode_capacity, "COBS decode respects capacity and preserves guards")
{
    const uint8_t cases[][4] = {{1}, {2, 0xAA}, {1, 1}, {2, 0xAA, 2, 0xBB}};
    const size_t sizes[] = {1, 2, 2, 4};
    const size_t needed[] = {0, 1, 1, 3};
    for (size_t i = 0; i < 4; ++i) {
        for (size_t capacity = 0; capacity <= needed[i] + 1; ++capacity) {
            uint8_t storage[8];
            memset(storage, 0xCC, sizeof(storage));
            size_t length = 99;
            frame_codec_result_t result = frame_cobs_decode(cases[i], sizes[i], storage + 1, capacity, &length);
            TEST_ASSERT_EQUAL_HEX8(0xCC, storage[0]);
            for (size_t j = capacity + 1; j < sizeof(storage); ++j) {
                TEST_ASSERT_EQUAL_HEX8(0xCC, storage[j]);
            }
            TEST_ASSERT_EQUAL_INT(capacity < needed[i] ? FRAME_CODEC_NO_SPACE : FRAME_CODEC_OK, result);
            TEST_ASSERT_EQUAL_UINT(capacity < needed[i] ? 0 : needed[i], length);
        }
    }
}

CODEC_CASE(test_round_trip, "COBS encode decode round trips deterministic payloads")
{
    uint8_t input[536], encoded[539], decoded[536];
    const size_t lengths[] = {0, 1, 2, 253, 254, 255, 508, 536};
    uint32_t state = 12345;
    for (unsigned pattern = 0; pattern < 4; ++pattern) {
        for (size_t k = 0; k < sizeof(lengths) / sizeof(lengths[0]); ++k) {
            const size_t n = lengths[k];
            for (size_t i = 0; i < n; ++i) {
                state = state * 1664525u + 1013904223u;
                input[i] = pattern == 0   ? 0
                           : pattern == 1 ? 0xAA
                           : pattern == 2 ? (i % 2 ? 0 : 0xFF)
                                          : (uint8_t)(state >> 24);
            }
            size_t encoded_len = 0, decoded_len = 0;
            TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_encode(input, n, encoded, sizeof(encoded), &encoded_len));
            TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_decode(encoded, encoded_len, decoded, n, &decoded_len));
            TEST_ASSERT_EQUAL_UINT(n, decoded_len);
            if (n) {
                TEST_ASSERT_EQUAL_MEMORY(input, decoded, n);
            }
        }
    }
}

CODEC_CASE(test_known_vectors, "COBS encode matches known vectors")
{
    static const struct {
        uint8_t input[4];
        size_t input_len;
        uint8_t expected[5];
        size_t expected_len;
    } vectors[] = {
        {{0}, 0, {1}, 1},
        {{0}, 1, {1, 1}, 2},
        {{0x11, 0x22}, 2, {3, 0x11, 0x22}, 3},
        {{0x11, 0, 0x22}, 3, {2, 0x11, 2, 0x22}, 4},
        {{0, 0x11}, 2, {1, 2, 0x11}, 3},
        {{0x11, 0}, 2, {2, 0x11, 1}, 3},
        {{0, 0}, 2, {1, 1, 1}, 3},
        {{1, 3, 0xFF}, 3, {4, 1, 3, 0xFF}, 4},
    };
    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
        uint8_t output[5];
        size_t length = 99;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_encode(vectors[i].input, vectors[i].input_len, output,
                                                                vectors[i].expected_len, &length));
        TEST_ASSERT_EQUAL_UINT(vectors[i].expected_len, length);
        TEST_ASSERT_EQUAL_MEMORY(vectors[i].expected, output, length);
    }
}

CODEC_CASE(test_arguments, "COBS encode validates pointers and empty input")
{
    uint8_t byte = 0xAA;
    size_t length = 99;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_cobs_encode(&byte, 1, &byte, 1, NULL));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_cobs_encode(NULL, 1, &byte, 1, &length));
    TEST_ASSERT_EQUAL_UINT(0, length);
    length = 99;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_cobs_encode(&byte, 1, NULL, 1, &length));
    TEST_ASSERT_EQUAL_UINT(0, length);
    length = 99;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_NO_SPACE, frame_cobs_encode(NULL, 0, &byte, 0, &length));
    TEST_ASSERT_EQUAL_UINT(0, length);
    TEST_ASSERT_EQUAL_HEX8(0xAA, byte);
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_encode(NULL, 0, &byte, 1, &length));
    TEST_ASSERT_EQUAL_UINT(1, length);
    TEST_ASSERT_EQUAL_HEX8(1, byte);
}

CODEC_CASE(test_block_boundaries, "COBS encode handles 254-byte block boundaries")
{
    const size_t lengths[] = {253, 254, 255, 508};
    uint8_t input[509];
    uint8_t expected[512];
    uint8_t output[512];
    memset(input, 0xAA, sizeof(input));
    for (size_t k = 0; k < sizeof(lengths) / sizeof(lengths[0]); ++k) {
        const size_t n = lengths[k];
        /* Build explicit full blocks and a final block, including an empty one. */
        size_t p = 0;
        for (size_t block = 0; block < n / 254; ++block) {
            expected[p++] = 0xFF;
            memset(expected + p, 0xAA, 254);
            p += 254;
        }
        expected[p++] = (uint8_t)(n % 254 + 1);
        memset(expected + p, 0xAA, n % 254);
        p += n % 254;
        size_t length = 0;
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_encode(input, n, output, p, &length));
        TEST_ASSERT_EQUAL_UINT(p, length);
        TEST_ASSERT_EQUAL_MEMORY(expected, output, p);
    }
    input[254] = 0;
    size_t length = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_encode(input, 255, output, 257, &length));
    TEST_ASSERT_EQUAL_UINT(257, length);
    TEST_ASSERT_EQUAL_HEX8(0xFF, output[0]);
    TEST_ASSERT_EQUAL_MEMORY(input, output + 1, 254);
    TEST_ASSERT_EQUAL_HEX8(1, output[255]);
    TEST_ASSERT_EQUAL_HEX8(1, output[256]);
}

CODEC_CASE(test_capacity_guards, "COBS encode respects capacity and clears failed length")
{
    uint8_t input[508];
    uint8_t storage[514];
    const size_t lengths[] = {0, 1, 253, 254, 255, 508};
    for (unsigned pattern = 0; pattern < 2; ++pattern) {
        memset(input, pattern ? 0xAA : 0, sizeof(input));
        for (size_t k = 0; k < sizeof(lengths) / sizeof(lengths[0]); ++k) {
            const size_t n = lengths[k];
            const size_t needed = n + 1 + (pattern ? n / 254 : 0);
            for (size_t capacity = 0; capacity <= needed + 1; ++capacity) {
                memset(storage, 0xCC, sizeof(storage));
                size_t length = 99;
                frame_codec_result_t result = frame_cobs_encode(input, n, storage + 1, capacity, &length);
                TEST_ASSERT_EQUAL_HEX8(0xCC, storage[0]);
                for (size_t j = capacity + 1; j < sizeof(storage); ++j) {
                    TEST_ASSERT_EQUAL_HEX8(0xCC, storage[j]);
                }
                if (capacity < needed) {
                    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_NO_SPACE, result);
                    TEST_ASSERT_EQUAL_UINT(0, length);
                } else {
                    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, result);
                    TEST_ASSERT_EQUAL_UINT(needed, length);
                }
            }
        }
    }
}
