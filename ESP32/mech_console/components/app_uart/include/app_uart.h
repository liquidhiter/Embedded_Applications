/*
 * @file    app_uart.h
 * @author  maxin
 * @date    2026-10-18
 * @brief   UART application interface.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"

/**
 * @brief   Initialize the application UART driver and pins.
 *
 *          This function does not create a receive task or an echo task.
 * @note    Not thread-safe; intended for a single initialization caller.
 *
 * @return
 *      - ESP_OK on success.
 *      - ESP_ERR_INVALID_STATE if already initialized.
 *      - A UART driver error code if initialization fails.
 */
esp_err_t app_uart_start(void);
