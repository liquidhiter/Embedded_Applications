/*
 * @file    app_link.h
 * @author  maxin
 * @date    2026-10-20
 * @brief   Application packet-link interface.
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

/**
 * @brief Send a frame packet over the application link.
 *
 * The link must be started with app_link_start() before calling this function.
 * Task-context only; concurrent send callers are supported after startup.
 * The caller must keep packet valid and unchanged until this function returns.
 * Each frame is submitted in one driver write, serialized by the UART TX mutex.
 * Other writers on this UART must also submit complete frames in one write.
 *
 * With the configured TX ring buffer, success means the complete frame has been
 * copied to the driver, not that transmission or reception has completed. Packet
 * storage may be reused after return. This call has no bounded timeout: driver
 * lock/buffer waits can block indefinitely. Use uart_wait_tx_done() separately
 * when local transmission completion is required.
 *
 * @param packet Pointer to the frame packet to send.
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if the link is not started,
 *         ESP_ERR_INVALID_ARG for a NULL packet or oversized payload, or ESP_FAIL
 *         for an encoding or UART write failure.
 */
esp_err_t app_link_send(const frame_packet_t *packet);
