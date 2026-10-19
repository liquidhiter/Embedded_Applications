/*
 * @file    app_uart.c
 * @author  maxin
 * @date    2026-10-18
 * @brief   UART initialization and optional echo test.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"
#include "driver/uart.h"

#include "app_uart.h"

/** @brief   Indicates whether the UART has been initialized. */
static bool uart_initialized = false;

/* Application tag for logging */
ESP_LOG_ATTR_TAG(TAG, "APP_UART");

/** UART configuration parameters */
#define APP_UART_NUM (UART_NUM_1)
#define APP_UART_BAUD_RATE (115200U)
#define APP_UART_DATA_BITS (UART_DATA_8_BITS)
#define APP_UART_PARITY (UART_PARITY_DISABLE)
#define APP_UART_STOP_BITS (UART_STOP_BITS_1)
#define APP_UART_FLOW_CTRL (UART_HW_FLOWCTRL_DISABLE)

#define APP_UART_RX_BUF_SIZE (2048U)
#define APP_UART_TX_BUF_SIZE (2048U)

#define APP_UART_TX_PIN (21U)
#define APP_UART_RX_PIN (20U)

#define APP_UART_ECHO_TEST
#undef APP_UART_ECHO_TEST

#ifdef APP_UART_ECHO_TEST

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void uart_echo_task(void *arg)
{
    (void)arg;

    uint8_t data[256] = {0};
    int read_len;
    int write_len;
    while (1) {
        int read_len = uart_read_bytes(APP_UART_NUM, data, sizeof(data), pdMS_TO_TICKS(100));
        if (read_len > 0) {
            ESP_LOGI(TAG, "Read %d bytes from UART", read_len);
            write_len = uart_write_bytes(APP_UART_NUM, (const char *)data, read_len);
            if (write_len < 0) {
                ESP_LOGE(TAG, "Failed to write UART data");
            }
            ESP_LOGI(TAG, "Wrote %d bytes to UART", write_len);
        }
    }
}
#endif

esp_err_t app_uart_start(void)
{
    esp_err_t ret;

    if (uart_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Initialize the UART */
    uart_config_t uart_config = {
        .baud_rate = APP_UART_BAUD_RATE,
        .data_bits = APP_UART_DATA_BITS,
        .parity = APP_UART_PARITY,
        .stop_bits = APP_UART_STOP_BITS,
        .flow_ctrl = APP_UART_FLOW_CTRL,
    };

    /* Install UART driver */
    ret = uart_driver_install(APP_UART_NUM,
                              APP_UART_RX_BUF_SIZE,
                              APP_UART_TX_BUF_SIZE,
                              0,
                              NULL,
                              0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }

    /** @NOTE: uart_driver_delete(APP_UART_NUM) needs to be called if any error occurs
     * after the UART driver has been installed.
     */

    /* Configure UART parameters */
    ret = uart_param_config(APP_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART parameters: %s", esp_err_to_name(ret));
        uart_driver_delete(APP_UART_NUM);
        return ret;
    }

    /* Configure UART pins */
    ret = uart_set_pin(APP_UART_NUM,
                       APP_UART_TX_PIN,
                       APP_UART_RX_PIN,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART pins: %s", esp_err_to_name(ret));
        uart_driver_delete(APP_UART_NUM);
        return ret;
    }

#ifdef APP_UART_ECHO_TEST
    /* Create a task for UART echo test */
    BaseType_t xReturned = xTaskCreate(uart_echo_task, "uart_echo_task", 2048, NULL, tskIDLE_PRIORITY, NULL);
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UART echo task");
        uart_driver_delete(APP_UART_NUM);
        return ESP_FAIL;
    }
#endif

    uart_initialized = true;

    return ESP_OK;

}
