/*
 * @file    app_uart.c
 * @author  maxin
 * @date    2026-10-18
 * @brief   Application UART driver initialization.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"

#include "driver/uart.h"

#include "app_uart.h"
#include "app_uart_config.h"

/** @brief   Indicates whether the UART has been initialized. */
static bool uart_initialized = false;

/* Application tag for logging */
ESP_LOG_ATTR_TAG(TAG, "APP_UART");

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
    ret = uart_driver_install(APP_UART_NUM, APP_UART_RX_BUF_SIZE, APP_UART_TX_BUF_SIZE, 0, NULL, 0);
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
    ret = uart_set_pin(APP_UART_NUM, APP_UART_TX_PIN, APP_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART pins: %s", esp_err_to_name(ret));
        uart_driver_delete(APP_UART_NUM);
        return ret;
    }

    uart_initialized = true;

    return ESP_OK;
}
