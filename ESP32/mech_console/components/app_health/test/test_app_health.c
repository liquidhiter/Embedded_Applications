#include "app_health.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

typedef struct {
    TaskHandle_t waiter;
    esp_err_t stop_result;
} stop_context_t;

static void request_stop(void *argument)
{
    stop_context_t *context = argument;
    context->stop_result = app_health_request_stop();
    xTaskNotifyGive(context->waiter);
}

TEST_CASE("Health task validates its one-shot lifecycle and timer stop request", "[app_health]")
{
    /* This module supports one lifecycle per boot; keep the sequence in one case. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, app_health_request_stop());
    TEST_ASSERT_EQUAL(ESP_OK, app_health_start());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, app_health_start());

    stop_context_t context = {
        .waiter = xTaskGetCurrentTaskHandle(),
        .stop_result = ESP_FAIL,
    };
    esp_timer_handle_t timer = NULL;
    const esp_timer_create_args_t args = {
        .callback = request_stop,
        .arg = &context,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "health_stop_test",
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_create(&args, &timer));
    esp_err_t result = esp_timer_start_once(timer, 5500000);
    if (result != ESP_OK) {
        esp_timer_delete(timer);
        app_health_request_stop();
        TEST_FAIL_MESSAGE("Could not start health-stop timer");
    }
    uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(8000));
    if (!notified) {
        /* Cancel the timer before the stack-based callback context expires. */
        esp_timer_stop(timer);
    }
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_delete(timer));
    TEST_ASSERT_TRUE_MESSAGE(notified > 0, "Health-stop callback timed out");
    TEST_ASSERT_EQUAL(ESP_OK, context.stop_result);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, app_health_request_stop());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, app_health_start());
    /* Acceptance of a stop request does not prove that idle-task cleanup finished. */
}
