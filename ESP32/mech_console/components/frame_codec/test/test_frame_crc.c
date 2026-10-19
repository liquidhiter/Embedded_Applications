#include <string.h>

#include "unity.h"

#include "frame_crc.h"

TEST_CASE("CRC32 matches the ISO-HDLC check vector", "[frame_crc]")
{
    const uint8_t input[] = "123456789";
    uint32_t crc = 0;
    /* Exclude the C string terminator from the standard nine-byte vector. */
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_crc32(input, sizeof(input) - 1, &crc));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926U, crc);
}

TEST_CASE("CRC32 accepts empty input and overwrites previous output", "[frame_crc]")
{
    const uint8_t dummy = 0xAA;
    uint32_t crc = 0x12345678U;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_crc32(NULL, 0, &crc));
    TEST_ASSERT_EQUAL_HEX32(0, crc);
    crc = 0x87654321U;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_crc32(&dummy, 0, &crc));
    TEST_ASSERT_EQUAL_HEX32(0, crc);
}

TEST_CASE("CRC32 rejects invalid pointers and clears failed output", "[frame_crc]")
{
    const uint8_t input = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_crc32(&input, 1, NULL));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_crc32(NULL, 0, NULL));
    uint32_t crc = 0xDEADBEEFU;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_crc32(NULL, 1, &crc));
    TEST_ASSERT_EQUAL_HEX32(0, crc);
}

TEST_CASE("CRC32 matches independent binary reference vectors", "[frame_crc]")
{
    /* Reference values generated with Python's zlib.crc32, not this implementation.
     * Columns represent all-zero, all-FF, and ascending modulo-256 byte patterns.
     */
    static const struct {
        size_t length;
        uint32_t expected[3];
    } vectors[] = {
        {1, {0xD202EF8DU, 0xFF000000U, 0xD202EF8DU}},   {2, {0x41D912FFU, 0xFFFF0000U, 0x36DE2269U}},
        {31, {0x6909ACB0U, 0x6CAB0B00U, 0x4D786D77U}},  {253, {0x181E4E5EU, 0x54760957U, 0xB367940EU}},
        {254, {0x5EC98D30U, 0x0A5BB25EU, 0xF6052BBFU}}, {255, {0xF48516ACU, 0x73D92741U, 0xD32F9BA0U}},
        {256, {0x0D968558U, 0xFEA8A821U, 0x29058C73U}}, {508, {0x0FBB6D54U, 0x4D3F5134U, 0x49F1A5EEU}},
        {512, {0xB2AA7578U, 0xBD7BC39FU, 0x1C613576U}}, {536, {0x11E5C2A9U, 0x58F2D1F0U, 0x24678442U}},
    };
    uint8_t input[536];
    for (size_t k = 0; k < sizeof(vectors) / sizeof(vectors[0]); ++k) {
        for (unsigned pattern = 0; pattern < 3; ++pattern) {
            for (size_t i = 0; i < vectors[k].length; ++i) {
                input[i] = pattern == 0 ? 0 : pattern == 1 ? 0xFF : (uint8_t)i;
            }
            uint32_t crc = 0x12345678U;
            TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_crc32(input, vectors[k].length, &crc));
            TEST_ASSERT_EQUAL_HEX32(vectors[k].expected[pattern], crc);
        }
    }
}

TEST_CASE("CRC32 handles maximum-sized deterministic data without mutation", "[frame_crc]")
{
    uint8_t input[538], original[538];
    uint32_t state = 12345;
    input[0] = input[537] = 0xCC;
    for (size_t i = 1; i <= 536; ++i) {
        state = state * 1664525U + 1013904223U;
        input[i] = (uint8_t)(state >> 24);
    }
    memcpy(original, input, sizeof(input));
    uint32_t crc = 0;
    /* Exercise a nonzero input offset and exclude both guard bytes. */
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_crc32(input + 1, 536, &crc));
    TEST_ASSERT_EQUAL_HEX32(0xD362D6DAU, crc);
    TEST_ASSERT_EQUAL_MEMORY(original, input, sizeof(input));
}

TEST_CASE("CRC32 detects every single-bit change in the check vector", "[frame_crc]")
{
    uint8_t input[] = "123456789";
    for (size_t i = 0; i < sizeof(input) - 1; ++i) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            input[i] ^= (uint8_t)(1U << bit);
            uint32_t crc = 0;
            TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_crc32(input, sizeof(input) - 1, &crc));
            TEST_ASSERT_TRUE_MESSAGE(crc != 0xCBF43926U, "Single-bit corruption went undetected");
            input[i] ^= (uint8_t)(1U << bit);
        }
    }
}
