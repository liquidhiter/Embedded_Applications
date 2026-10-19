#include <string.h>

#include "unity.h"

#include "frame_stream.h"

typedef struct {
    size_t count;
    size_t lengths[4];
    uint8_t frames[4][FRAME_STREAM_MAX_ENCODED_SIZE];
} capture_t;

static void capture(const uint8_t *frame, size_t length, void *context)
{
    capture_t *result = context;
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(result->count < 4);
    TEST_ASSERT_TRUE(length > 0 && length <= FRAME_STREAM_MAX_ENCODED_SIZE);
    result->lengths[result->count] = length;
    memcpy(result->frames[result->count++], frame, length);
}

static void start(frame_stream_t *stream, capture_t *result)
{
    memset(result, 0, sizeof(*result));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_init(stream, capture, result));
}

TEST_CASE("Stream delivers a complete frame without delimiter", "[frame_stream]")
{
    frame_stream_t stream;
    capture_t result;
    start(&stream, &result);
    const uint8_t input[] = {2, 0x11, 2, 0x22, 0};
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, input, sizeof(input)));
    TEST_ASSERT_EQUAL_UINT(1, result.count);
    TEST_ASSERT_EQUAL_UINT(4, result.lengths[0]);
    TEST_ASSERT_EQUAL_MEMORY(input, result.frames[0], 4);
    TEST_ASSERT_EQUAL_UINT(0, stream.length);
}

TEST_CASE("Stream preserves partial frames across every split", "[frame_stream]")
{
    const uint8_t input[] = {2, 0x11, 2, 0x22, 0};
    for (size_t split = 0; split <= sizeof(input); ++split) {
        frame_stream_t stream;
        capture_t result;
        start(&stream, &result);
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, input, split));
        TEST_ASSERT_EQUAL_UINT(split == sizeof(input) ? 1 : 0, result.count);
        TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, input + split, sizeof(input) - split));
        TEST_ASSERT_EQUAL_UINT(1, result.count);
        TEST_ASSERT_EQUAL_UINT(4, result.lengths[0]);
        TEST_ASSERT_EQUAL_MEMORY(input, result.frames[0], 4);
    }
}

TEST_CASE("Stream handles bytewise input and multiple frames", "[frame_stream]")
{
    const uint8_t input[] = {2, 0x11, 0, 3, 0x22, 0x33, 0};
    for (unsigned bytewise = 0; bytewise < 2; ++bytewise) {
        frame_stream_t stream;
        capture_t result;
        start(&stream, &result);
        if (bytewise) {
            for (size_t i = 0; i < sizeof(input); ++i) {
                TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, input + i, 1));
            }
        } else {
            TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, input, sizeof(input)));
        }
        TEST_ASSERT_EQUAL_UINT(2, result.count);
        TEST_ASSERT_EQUAL_UINT(2, result.lengths[0]);
        TEST_ASSERT_EQUAL_UINT(3, result.lengths[1]);
        TEST_ASSERT_EQUAL_MEMORY(input, result.frames[0], 2);
        TEST_ASSERT_EQUAL_MEMORY(input + 3, result.frames[1], 3);
    }
}

TEST_CASE("Stream ignores empty frames and retains trailing partial data", "[frame_stream]")
{
    frame_stream_t stream;
    capture_t result;
    start(&stream, &result);
    const uint8_t input[] = {0, 0, 2, 0x11, 0, 0, 3, 0x22};
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, input, sizeof(input)));
    TEST_ASSERT_EQUAL_UINT(1, result.count);
    TEST_ASSERT_EQUAL_UINT(2, stream.length);
    const uint8_t tail[] = {0x33, 0, 0};
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, tail, sizeof(tail)));
    const uint8_t expected[] = {3, 0x22, 0x33};
    TEST_ASSERT_EQUAL_UINT(2, result.count);
    TEST_ASSERT_EQUAL_UINT(3, result.lengths[1]);
    TEST_ASSERT_EQUAL_MEMORY(expected, result.frames[1], sizeof(expected));
}

TEST_CASE("Stream accepts exactly the maximum frame length", "[frame_stream]")
{
    frame_stream_t stream;
    capture_t result;
    start(&stream, &result);
    uint8_t input[FRAME_STREAM_MAX_ENCODED_SIZE + 1];
    memset(input, 0xAA, sizeof(input));
    input[FRAME_STREAM_MAX_ENCODED_SIZE] = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, input, FRAME_STREAM_MAX_ENCODED_SIZE));
    TEST_ASSERT_EQUAL_UINT(0, result.count);
    TEST_ASSERT_FALSE(stream.discarding);
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, input + FRAME_STREAM_MAX_ENCODED_SIZE, 1));
    TEST_ASSERT_EQUAL_UINT(1, result.count);
    TEST_ASSERT_EQUAL_UINT(FRAME_STREAM_MAX_ENCODED_SIZE, result.lengths[0]);
    TEST_ASSERT_EQUAL_MEMORY(input, result.frames[0], FRAME_STREAM_MAX_ENCODED_SIZE);
}

TEST_CASE("Stream discards an oversized frame and recovers in the same chunk", "[frame_stream]")
{
    frame_stream_t stream;
    capture_t result;
    start(&stream, &result);
    uint8_t input[FRAME_STREAM_MAX_ENCODED_SIZE + 8];
    memset(input, 0xAA, sizeof(input));
    size_t p = FRAME_STREAM_MAX_ENCODED_SIZE + 3;
    input[p++] = 0;
    input[p++] = 2;
    input[p++] = 0x11;
    input[p++] = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, input, p));
    const uint8_t expected[] = {2, 0x11};
    TEST_ASSERT_EQUAL_UINT(1, result.count);
    TEST_ASSERT_EQUAL_UINT(2, result.lengths[0]);
    TEST_ASSERT_EQUAL_MEMORY(expected, result.frames[0], 2);
    TEST_ASSERT_FALSE(stream.discarding);
    TEST_ASSERT_EQUAL_UINT(0, stream.length);
}

TEST_CASE("Stream preserves discard state across calls", "[frame_stream]")
{
    frame_stream_t stream;
    capture_t result;
    start(&stream, &result);
    uint8_t input[FRAME_STREAM_MAX_ENCODED_SIZE + 1];
    memset(input, 0xAA, sizeof(input));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, input, sizeof(input)));
    TEST_ASSERT_TRUE(stream.discarding);
    const uint8_t ignored[] = {0xBB, 0xCC};
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, ignored, sizeof(ignored)));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, NULL, 0));
    TEST_ASSERT_TRUE(stream.discarding);
    TEST_ASSERT_EQUAL_UINT(0, stream.length);
    TEST_ASSERT_EQUAL_UINT(0, result.count);
    const uint8_t recovery[] = {0, 0, 2, 0x33, 0};
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, recovery, sizeof(recovery)));
    TEST_ASSERT_EQUAL_UINT(1, result.count);
    TEST_ASSERT_EQUAL_UINT(2, result.lengths[0]);
    TEST_ASSERT_EQUAL_MEMORY(recovery + 2, result.frames[0], 2);
    TEST_ASSERT_FALSE(stream.discarding);
}

TEST_CASE("Stream validates arguments and can be reinitialized", "[frame_stream]")
{
    frame_stream_t stream;
    capture_t result;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_stream_init(NULL, capture, &result));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_stream_init(&stream, NULL, &result));
    start(&stream, &result);
    const uint8_t byte = 2;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_stream_feed(NULL, &byte, 1));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_stream_feed(&stream, NULL, 1));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, NULL, 0));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, &byte, 0));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_feed(&stream, &byte, 1));
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_INVALID_ARG, frame_stream_feed(&stream, NULL, 1));
    TEST_ASSERT_EQUAL_UINT(1, stream.length);
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_stream_init(&stream, capture, NULL));
    TEST_ASSERT_EQUAL_UINT(0, stream.length);
    TEST_ASSERT_FALSE(stream.discarding);
    TEST_ASSERT_NULL(stream.context);
    TEST_ASSERT_EQUAL_PTR(capture, stream.callback);
    TEST_ASSERT_EQUAL_UINT(0, result.count);
}
