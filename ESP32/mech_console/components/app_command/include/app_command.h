/**
 * @file    app_command.h
 * @author  maxin
 * @date    2026-11-01
 * @brief   Asynchronous command submission interface.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"

#include "frame_packet.h"

/**
 * @brief Create the four-packet command queue and its worker task.
 *
 * Startup is not thread-safe; use one initialization caller. Start this module
 * before registering the UART receive callback, and initialize app_link before
 * submitting requests that require a response. There is no stop API.
 *
 * @return ESP_OK on success; ESP_ERR_INVALID_STATE if already started;
 *         ESP_ERR_NO_MEM if queue/task allocation fails. Failed starts release
 *         their resources and may be retried.
 */
esp_err_t app_command_start(void);

/**
 * @brief Copy a request into the bounded FIFO without waiting for queue space.
 *
 * Task-context only; concurrent producers are supported after successful startup.
 * The caller must keep request valid and unchanged during this call. After a
 * successful return, request storage can be reused: the full packet was copied.
 * A full queue rejects the new request without overwriting queued requests.
 * Diagnostic logging can take time; zero queue wait is not a latency guarantee.
 *
 * @param request Required pointer to a decoded packet.
 * @return ESP_OK means accepted into the queue, not execution or response
 *         completion. ESP_ERR_INVALID_STATE means not started;
 *         ESP_ERR_INVALID_ARG means NULL or excessive payload length;
 *         ESP_ERR_TIMEOUT means the queue is full. Rejected requests are not sent
 *         to the protocol handler and do not generate a wire error response.
 */
esp_err_t app_command_submit(const frame_packet_t *request);
