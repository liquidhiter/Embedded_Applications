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
 * @brief   Start the UART application.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL on failure
 */
esp_err_t app_uart_start(void);
