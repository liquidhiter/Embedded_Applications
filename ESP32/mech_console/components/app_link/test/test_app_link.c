#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "unity.h"

#include "app_link.h"
#include "app_uart_config.h"
#include "frame_codec.h"
#include "frame_crc.h"
#include "frame_stream.h"

typedef struct {
    int length;
    uint8_t bytes[256];
} input_chunk_t;

typedef struct {
    frame_packet_t packet;
    bool context_matches;
} received_packet_t;

static QueueHandle_t input_queue;
static QueueHandle_t packet_queue;
static int callback_context;

int __real_uart_read_bytes(uart_port_t port, void *buffer, uint32_t length, TickType_t wait);

/* This seam replaces the driver, not the receive task, framer, or packet codec. */
int __wrap_uart_read_bytes(uart_port_t port, void *buffer, uint32_t length, TickType_t wait)
{
    if (port != APP_UART_NUM) {
        return __real_uart_read_bytes(port, buffer, length, wait);
    }
    input_chunk_t chunk;
    if (xQueueReceive(input_queue, &chunk, wait) != pdTRUE) {
        return 0;
    }
    if (chunk.length < 0) {
        return -1;
    }
    configASSERT((uint32_t)chunk.length <= length);
    memcpy(buffer, chunk.bytes, (size_t)chunk.length);
    return chunk.length;
}

static void on_packet(const frame_packet_t *packet, void *context)
{
    received_packet_t received = {.context_matches = context == &callback_context};
    memcpy(&received.packet, packet, sizeof(*packet));
    /* Never call Unity assertions from the receive task. */
    configASSERT(xQueueSend(packet_queue, &received, 0) == pdTRUE);
}

static void start_link(void)
{
    input_queue = xQueueCreate(8, sizeof(input_chunk_t));
    packet_queue = xQueueCreate(8, sizeof(received_packet_t));
    TEST_ASSERT_NOT_NULL(input_queue);
    TEST_ASSERT_NOT_NULL(packet_queue);
    TEST_ASSERT_EQUAL_INT(ESP_OK, app_link_start(on_packet, &callback_context));
    /* Queues remain alive because the production task has no stop API.
     * Each pytest item boots a fresh image before selecting its Unity case.
     */
}

static void send_bytes(const uint8_t *data, size_t length, size_t chunk_size)
{
    while (length != 0) {
        input_chunk_t chunk = {0};
        size_t count = length < chunk_size ? length : chunk_size;
        TEST_ASSERT_TRUE(count <= sizeof(chunk.bytes));
        chunk.length = (int)count;
        memcpy(chunk.bytes, data, count);
        TEST_ASSERT_EQUAL(pdTRUE, xQueueSend(input_queue, &chunk, pdMS_TO_TICKS(2000)));
        data += count;
        length -= count;
    }
}

static frame_packet_t make_packet(uint32_t sequence, uint16_t length)
{
    frame_packet_t packet = {
        .type = 0x20,
        .session_id = UINT64_C(0x8877665544332211),
        .sequence = sequence,
        .payload_length = length,
    };
    for (size_t i = 0; i < length; ++i) {
        packet.payload[i] = (uint8_t)i;
    }
    return packet;
}

static size_t encode(const frame_packet_t *packet, uint8_t *wire)
{
    size_t length = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_packet_encode(packet, wire, 540, &length));
    return length;
}

static void expect_packet(const frame_packet_t *expected)
{
    received_packet_t received;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(packet_queue, &received, pdMS_TO_TICKS(2000)));
    TEST_ASSERT_TRUE(received.context_matches);
    TEST_ASSERT_EQUAL_HEX8(expected->type, received.packet.type);
    TEST_ASSERT_EQUAL_MEMORY(&expected->session_id, &received.packet.session_id, sizeof(expected->session_id));
    TEST_ASSERT_EQUAL_UINT32(expected->sequence, received.packet.sequence);
    TEST_ASSERT_EQUAL_UINT16(expected->payload_length, received.packet.payload_length);
    if (expected->payload_length != 0) {
        TEST_ASSERT_EQUAL_MEMORY(expected->payload, received.packet.payload, expected->payload_length);
    }
}

static void expect_no_extra_packets(void)
{
    received_packet_t unexpected;
    TEST_ASSERT_EQUAL(pdFALSE, xQueueReceive(packet_queue, &unexpected, pdMS_TO_TICKS(50)));
}

TEST_CASE("Link rejects null and duplicate starts", "[app_link]")
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, app_link_start(NULL, NULL));
    start_link();
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, app_link_start(on_packet, NULL));
    frame_packet_t expected = make_packet(1, 0);
    uint8_t wire[540];
    size_t length = encode(&expected, wire);
    send_bytes(wire, length, 256);
    expect_packet(&expected);
    expect_no_extra_packets();
}

TEST_CASE("Link preserves partial frames across reads and timeouts", "[app_link]")
{
    start_link();
    frame_packet_t expected = make_packet(2, 3);
    uint8_t wire[540];
    size_t length = encode(&expected, wire);
    send_bytes(wire, 2, 1);
    /* Longer than the UART read timeout; a timeout must not reset framing. */
    vTaskDelay(pdMS_TO_TICKS(200));
    expect_no_extra_packets();
    send_bytes(wire + 2, length - 3, 1);
    expect_no_extra_packets();
    send_bytes(wire + length - 1, 1, 1);
    expect_packet(&expected);
    expect_no_extra_packets();
}

TEST_CASE("Link delivers multiple frames from one read in order", "[app_link]")
{
    start_link();
    frame_packet_t first = make_packet(3, 0), second = make_packet(4, 3);
    uint8_t wire[1080];
    size_t length = encode(&first, wire);
    length += encode(&second, wire + length);
    send_bytes(wire, length, 256);
    expect_packet(&first);
    expect_packet(&second);
    expect_no_extra_packets();
}

TEST_CASE("Link drops invalid packets and resumes at the next frame", "[app_link]")
{
    start_link();
    const uint8_t malformed[] = {0, 0, 3, 0x11, 0};
    send_bytes(malformed, sizeof(malformed), 256);
    /* Valid COBS and header, but deliberately incorrect CRC. */
    uint8_t raw[24] = {1}, wire[540];
    uint32_t crc = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_crc32(raw, 20, &crc));
    crc ^= 1U;
    for (size_t i = 0; i < 4; ++i) {
        raw[20 + i] = (uint8_t)(crc >> (8 * i));
    }
    size_t length = 0;
    TEST_ASSERT_EQUAL_INT(FRAME_CODEC_OK, frame_cobs_encode(raw, sizeof(raw), wire, sizeof(wire) - 1, &length));
    wire[length++] = 0;
    send_bytes(wire, length, 256);
    frame_packet_t expected = make_packet(5, 3);
    length = encode(&expected, wire);
    send_bytes(wire, length, 256);
    expect_packet(&expected);
    expect_no_extra_packets();
}

TEST_CASE("Link discards oversized frames then delivers a maximum packet", "[app_link]")
{
    start_link();
    uint8_t oversized[FRAME_STREAM_MAX_ENCODED_SIZE + 2];
    memset(oversized, 1, sizeof(oversized));
    oversized[sizeof(oversized) - 1] = 0;
    send_bytes(oversized, sizeof(oversized), 127);
    frame_packet_t expected = make_packet(6, 512);
    uint8_t wire[540];
    size_t length = encode(&expected, wire);
    send_bytes(wire, length, 256);
    expect_packet(&expected);
    expect_no_extra_packets();
}

TEST_CASE("Link retries after a UART read failure", "[app_link]")
{
    start_link();
    input_chunk_t failure = {.length = -1};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueSend(input_queue, &failure, pdMS_TO_TICKS(2000)));
    frame_packet_t expected = make_packet(7, 3);
    uint8_t wire[540];
    size_t length = encode(&expected, wire);
    send_bytes(wire, length, 256);
    expect_packet(&expected);
    expect_no_extra_packets();
}
