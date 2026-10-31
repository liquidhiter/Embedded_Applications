#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

#include "app_link.h"
#include "app_protocol.h"
#include "app_protocol_defs.h"

static bool intercept_send;
static bool fake_stats;
static TaskHandle_t test_task;
static frame_packet_t captured_response;
static unsigned send_calls;
static bool sent_null;
static esp_err_t send_result;
static uint64_t uptime_ms;
static size_t free_heap;
static size_t min_heap;

esp_err_t __real_app_link_send(const frame_packet_t *packet);
int64_t __real_esp_timer_get_time(void);
size_t __real_xPortGetFreeHeapSize(void);
size_t __real_xPortGetMinimumEverFreeHeapSize(void);

static bool use_test_stats(void)
{
    /* Preserve real clock/heap observations in unrelated tasks and interrupts. */
    return fake_stats && !xPortInIsrContext() && xTaskGetCurrentTaskHandle() == test_task;
}

int64_t __wrap_esp_timer_get_time(void)
{
    return use_test_stats() ? (int64_t)(uptime_ms * 1000 + 999) : __real_esp_timer_get_time();
}

size_t __wrap_xPortGetFreeHeapSize(void)
{
    return use_test_stats() ? free_heap : __real_xPortGetFreeHeapSize();
}

size_t __wrap_xPortGetMinimumEverFreeHeapSize(void)
{
    return use_test_stats() ? min_heap : __real_xPortGetMinimumEverFreeHeapSize();
}

esp_err_t __wrap_app_link_send(const frame_packet_t *packet)
{
    if (!intercept_send) {
        return __real_app_link_send(packet);
    }
    ++send_calls;
    sent_null = packet == NULL;
    if (packet != NULL) {
        memcpy(&captured_response, packet, sizeof(*packet));
    }
    return send_result;
}

static void reset_capture(void)
{
    send_calls = 0;
    sent_null = false;
    send_result = ESP_OK;
    memset(&captured_response, 0xA5, sizeof(captured_response));
    uptime_ms = UINT64_C(0x010203040506);
    free_heap = 0x54321;
    min_heap = 0x12345;
}

static esp_err_t dispatch(const frame_packet_t *request)
{
    test_task = xTaskGetCurrentTaskHandle();
    intercept_send = true;
    fake_stats = true;
    esp_err_t result = app_protocol_handle_packet(request);
    fake_stats = false;
    intercept_send = false;
    return result;
}

TEST_CASE("Protocol rejects null requests without sending", "[app_protocol]")
{
    reset_capture();
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, dispatch(NULL));
    TEST_ASSERT_EQUAL_UINT(0, send_calls);
}

TEST_CASE("Protocol rejects unknown types and dispatch table holes", "[app_protocol]")
{
    reset_capture();
    frame_packet_t request = {0};
    for (unsigned type = 0; type <= UINT8_MAX; ++type) {
        if (type == APP_PACKET_TYPE_GET_STATUS) {
            continue;
        }
        request.type = (uint8_t)type;
        TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_SUPPORTED, dispatch(&request));
    }
    TEST_ASSERT_EQUAL_UINT(0, send_calls);
}

TEST_CASE("Protocol rejects nonempty status requests without sending", "[app_protocol]")
{
    reset_capture();
    frame_packet_t request = {.type = APP_PACKET_TYPE_GET_STATUS};
    const uint16_t lengths[] = {1, 16, 512, UINT16_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        request.payload_length = lengths[i];
        TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, dispatch(&request));
    }
    TEST_ASSERT_EQUAL_UINT(0, send_calls);
}

TEST_CASE("Protocol status uses the exact little endian payload layout", "[app_protocol]")
{
    reset_capture();
    frame_packet_t request = {
        .type = 0x01,
        .session_id = UINT64_C(0x8877665544332211),
        .sequence = UINT32_C(0xFEDCBA98),
    };
    /* Independent byte reference: no production helpers decode the response.
     * Uptime exceeds 32 bits and mocked microseconds have a sub-ms remainder.
     */
    const uint8_t expected[] = {
        0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x00, 0x21, 0x43, 0x05, 0x00, 0x45, 0x23, 0x01, 0x00,
    };
    uint8_t original[sizeof(request)];
    memcpy(original, &request, sizeof(request));
    TEST_ASSERT_EQUAL_INT(ESP_OK, dispatch(&request));
    TEST_ASSERT_EQUAL_UINT(1, send_calls);
    TEST_ASSERT_FALSE(sent_null);
    TEST_ASSERT_EQUAL_HEX8(0x81, captured_response.type);
    TEST_ASSERT_EQUAL_UINT16(sizeof(expected), captured_response.payload_length);
    TEST_ASSERT_EQUAL_MEMORY(expected, captured_response.payload, sizeof(expected));
    TEST_ASSERT_EQUAL_MEMORY(&request.session_id, &captured_response.session_id, sizeof(request.session_id));
    TEST_ASSERT_EQUAL_HEX32(request.sequence, captured_response.sequence);
    TEST_ASSERT_EQUAL_MEMORY(original, &request, sizeof(request));
}

TEST_CASE("Protocol status preserves zero and maximum request identifiers", "[app_protocol]")
{
    frame_packet_t request = {.type = APP_PACKET_TYPE_GET_STATUS};
    for (unsigned i = 0; i < 2; ++i) {
        reset_capture();
        request.session_id = i == 0 ? 0 : UINT64_MAX;
        request.sequence = i == 0 ? 0 : UINT32_MAX;
        uptime_ms = 0;
        free_heap = 0;
        min_heap = 0;
        const uint8_t zeros[16] = {0};
        TEST_ASSERT_EQUAL_INT(ESP_OK, dispatch(&request));
        TEST_ASSERT_EQUAL_UINT(1, send_calls);
        TEST_ASSERT_EQUAL_MEMORY(&request.session_id, &captured_response.session_id, sizeof(request.session_id));
        TEST_ASSERT_EQUAL_HEX32(request.sequence, captured_response.sequence);
        TEST_ASSERT_EQUAL_MEMORY(zeros, captured_response.payload, sizeof(zeros));
    }
}

TEST_CASE("Protocol propagates send failures without retrying", "[app_protocol]")
{
    frame_packet_t request = {.type = APP_PACKET_TYPE_GET_STATUS};
    const esp_err_t errors[] = {ESP_ERR_INVALID_STATE, ESP_ERR_NO_MEM, ESP_FAIL};
    for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
        reset_capture();
        send_result = errors[i];
        TEST_ASSERT_EQUAL_INT(errors[i], dispatch(&request));
        TEST_ASSERT_EQUAL_UINT(1, send_calls);
    }
    reset_capture();
    TEST_ASSERT_EQUAL_INT(ESP_OK, dispatch(&request));
    TEST_ASSERT_EQUAL_UINT(1, send_calls);
}
