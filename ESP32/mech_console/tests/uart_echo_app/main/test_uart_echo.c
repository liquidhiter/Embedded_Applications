#include <stdlib.h>
#include "app_uart.h"
#include "app_uart_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "UART_ECHO_TEST";

static void echo_task(void *context)
{
    (void)context;
    uint8_t buffer[256];
    ESP_ERROR_CHECK(uart_flush_input(APP_UART_NUM));
    ESP_LOGI(TAG, "UART_ECHO_READY");
    for (;;) {
        int count = uart_read_bytes(APP_UART_NUM, buffer, sizeof(buffer), pdMS_TO_TICKS(100));
        if (count < 0) {
            ESP_LOGE(TAG, "UART read failed");
            abort();
        }
        int offset = 0;
        while (offset < count) {
            int written = uart_write_bytes(APP_UART_NUM, buffer + offset, count - offset);
            if (written <= 0) {
                ESP_LOGE(TAG, "UART write failed");
                abort();
            }
            offset += written;
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(app_uart_start());
    if (xTaskCreate(echo_task, "uart_echo_test", 3072, NULL, 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not create echo task");
        abort();
    }
}
