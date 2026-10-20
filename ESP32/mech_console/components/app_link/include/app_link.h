/*
 * @file    app_link.h
 * @author  maxin
 * @date    2026-10-20
 * @brief   Application receive-link interface.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"

#include "frame_packet.h"

/**
 * @brief Callback type for handling received frame packets.
 *
 * Runs synchronously on the receive task. Return promptly and do not retain
 * packet; copy it if needed after the callback returns. Context must remain
 * valid for the lifetime of the link. Invalid frames never invoke this callback.
 *
 * @param packet  Pointer to the received frame packet.
 * @param context User-defined context passed to the callback.
 */
typedef void (*app_link_packet_callback_t)(const frame_packet_t *packet, void *context);

/**
 * @brief Start the application link with the specified packet callback and context.
 *
 * Call app_uart_start() successfully first. This function is not thread-safe;
 * use a single initialization caller. The link owns UART reception exclusively.
 * There is no stop API; the receive task runs until reboot. UART read errors
 * are logged and retried after a delay; normal timeouts immediately resume reading.
 * The callback may execute before this function returns.
 *
 * @param callback Callback function to handle received frame packets.
 * @param context  User-defined context passed to the callback.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a NULL callback,
 *         ESP_ERR_INVALID_STATE if already started or stream initialization fails,
 *         or ESP_ERR_NO_MEM if task creation fails. Failed starts may be retried.
 */
esp_err_t app_link_start(app_link_packet_callback_t callback, void *context);
