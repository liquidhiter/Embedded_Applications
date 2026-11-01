#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "unity.h"

#include "app_command.h"
#include "app_protocol.h"

static bool testing_start;
static bool intercept_handler;
static bool fail_queue;
static bool fail_task;
static QueueHandle_t created_queue;
static unsigned deleted_queues;
static TaskHandle_t worker;
static QueueHandle_t observed;
static SemaphoreHandle_t gate;

QueueHandle_t __real_xQueueGenericCreate(UBaseType_t length, UBaseType_t size, uint8_t type);
void __real_vQueueDelete(QueueHandle_t queue);
BaseType_t __real_xTaskCreatePinnedToCore(TaskFunction_t function, const char *name, configSTACK_DEPTH_TYPE stack,
                                          void *context, UBaseType_t priority, TaskHandle_t *handle, BaseType_t core);
esp_err_t __real_app_protocol_handle_packet(const frame_packet_t *packet);

QueueHandle_t __wrap_xQueueGenericCreate(UBaseType_t length, UBaseType_t size, uint8_t type)
{
    bool command_queue = testing_start && length == 4 && size == sizeof(frame_packet_t);
    if (command_queue && fail_queue) {
        return NULL;
    }
    QueueHandle_t queue = __real_xQueueGenericCreate(length, size, type);
    if (command_queue) {
        created_queue = queue;
    }
    return queue;
}

void __wrap_vQueueDelete(QueueHandle_t queue)
{
    if (testing_start && queue == created_queue) {
        ++deleted_queues;
    }
    __real_vQueueDelete(queue);
}

BaseType_t __wrap_xTaskCreatePinnedToCore(TaskFunction_t function, const char *name, configSTACK_DEPTH_TYPE stack,
                                          void *context, UBaseType_t priority, TaskHandle_t *handle, BaseType_t core)
{
    bool command_task = testing_start && strcmp(name, "command_task") == 0;
    if (command_task && fail_task) {
        if (handle != NULL) {
            *handle = NULL;
        }
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }
    BaseType_t result = __real_xTaskCreatePinnedToCore(function, name, stack, context, priority, handle, core);
    if (command_task && result == pdPASS) {
        worker = *handle;
    }
    return result;
}

esp_err_t __wrap_app_protocol_handle_packet(const frame_packet_t *packet)
{
    if (!intercept_handler || xTaskGetCurrentTaskHandle() != worker) {
        return __real_app_protocol_handle_packet(packet);
    }
    /* Use real queues/tasks. Only execution is gated so saturation is deterministic. */
    configASSERT(xQueueSend(observed, packet, portMAX_DELAY) == pdTRUE);
    configASSERT(xSemaphoreTake(gate, portMAX_DELAY) == pdTRUE);
    return packet->sequence == 1 ? ESP_ERR_INVALID_ARG : packet->sequence == 2 ? ESP_FAIL : ESP_OK;
}

static void prepare(void)
{
    observed = xQueueCreate(8, sizeof(frame_packet_t));
    gate = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(observed);
    TEST_ASSERT_NOT_NULL(gate);
    intercept_handler = true;
    /* The command task has no stop API. pytest boots afresh for each Unity case. */
}

static esp_err_t start(void)
{
    testing_start = true;
    esp_err_t result = app_command_start();
    testing_start = false;
    return result;
}

static frame_packet_t packet(uint32_t sequence)
{
    frame_packet_t result = {
        .type = 1,
        .session_id = UINT64_C(0x8877665544332211),
        .sequence = sequence,
        .payload_length = 512,
    };
    for (size_t i = 0; i < sizeof(result.payload); ++i) {
        result.payload[i] = (uint8_t)(i + sequence);
    }
    return result;
}

static void expect_packet(const frame_packet_t *expected)
{
    frame_packet_t actual;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(observed, &actual, pdMS_TO_TICKS(2000)));
    TEST_ASSERT_EQUAL_HEX8(expected->type, actual.type);
    TEST_ASSERT_EQUAL_MEMORY(&expected->session_id, &actual.session_id, sizeof(actual.session_id));
    TEST_ASSERT_EQUAL_UINT32(expected->sequence, actual.sequence);
    TEST_ASSERT_EQUAL_UINT16(expected->payload_length, actual.payload_length);
    TEST_ASSERT_EQUAL_MEMORY(expected->payload, actual.payload, expected->payload_length);
}

static void release_worker(void)
{
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(gate));
}

TEST_CASE("Command rejects invalid submissions and duplicate startup", "[app_command]")
{
    prepare();
    frame_packet_t request = packet(3);
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, app_command_submit(&request));
    TEST_ASSERT_EQUAL_INT(ESP_OK, start());
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, start());
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, app_command_submit(NULL));
    request.payload_length = 513;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, app_command_submit(&request));
    TEST_ASSERT_EQUAL_UINT(0, uxQueueMessagesWaiting(observed));
}

TEST_CASE("Command copies queued data and retains FIFO order", "[app_command]")
{
    prepare();
    TEST_ASSERT_EQUAL_INT(ESP_OK, start());
    frame_packet_t blocker = packet(3);
    TEST_ASSERT_EQUAL_INT(ESP_OK, app_command_submit(&blocker));
    expect_packet(&blocker);
    /* Hold the worker while reusing the producer's stack buffer. */
    frame_packet_t request = packet(4);
    TEST_ASSERT_EQUAL_INT(ESP_OK, app_command_submit(&request));
    request = packet(5);
    TEST_ASSERT_EQUAL_INT(ESP_OK, app_command_submit(&request));
    memset(&request, 0xA5, sizeof(request));
    release_worker();
    frame_packet_t expected = packet(4);
    expect_packet(&expected);
    release_worker();
    expected = packet(5);
    expect_packet(&expected);
    release_worker();
}

TEST_CASE("Command rejects full queues then accepts requests after draining", "[app_command]")
{
    prepare();
    TEST_ASSERT_EQUAL_INT(ESP_OK, start());
    frame_packet_t request = packet(3);
    TEST_ASSERT_EQUAL_INT(ESP_OK, app_command_submit(&request));
    expect_packet(&request);
    for (uint32_t sequence = 4; sequence < 8; ++sequence) {
        request = packet(sequence);
        TEST_ASSERT_EQUAL_INT(ESP_OK, app_command_submit(&request));
    }
    request = packet(99);
    TEST_ASSERT_EQUAL_INT(ESP_ERR_TIMEOUT, app_command_submit(&request));
    /* Verify failed submission did not overwrite any queued packet. */
    for (uint32_t sequence = 4; sequence < 8; ++sequence) {
        release_worker();
        request = packet(sequence);
        expect_packet(&request);
    }
    request = packet(8);
    TEST_ASSERT_EQUAL_INT(ESP_OK, app_command_submit(&request));
    release_worker();
    expect_packet(&request);
    release_worker();
}

TEST_CASE("Command continues processing after handler errors", "[app_command]")
{
    prepare();
    TEST_ASSERT_EQUAL_INT(ESP_OK, start());
    for (uint32_t sequence = 1; sequence <= 3; ++sequence) {
        frame_packet_t request = packet(sequence);
        TEST_ASSERT_EQUAL_INT(ESP_OK, app_command_submit(&request));
        expect_packet(&request);
        release_worker();
    }
}

TEST_CASE("Command startup retries after queue allocation failure", "[app_command]")
{
    prepare();
    fail_queue = true;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NO_MEM, start());
    fail_queue = false;
    frame_packet_t request = packet(3);
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, app_command_submit(&request));
    TEST_ASSERT_EQUAL_INT(ESP_OK, start());
    TEST_ASSERT_EQUAL_INT(ESP_OK, app_command_submit(&request));
    expect_packet(&request);
    release_worker();
}

TEST_CASE("Command startup frees its queue when task allocation fails", "[app_command]")
{
    prepare();
    fail_task = true;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NO_MEM, start());
    TEST_ASSERT_NOT_NULL(created_queue);
    TEST_ASSERT_EQUAL_UINT(1, deleted_queues);
    fail_task = false;
    frame_packet_t request = packet(3);
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, app_command_submit(&request));
    TEST_ASSERT_EQUAL_INT(ESP_OK, start());
    TEST_ASSERT_EQUAL_INT(ESP_OK, app_command_submit(&request));
    expect_packet(&request);
    release_worker();
}
