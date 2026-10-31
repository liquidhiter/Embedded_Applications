/*
 * @file    app_protocol.h
 * @author  maxin
 * @date    2026-10-31
 * @brief   Application command dispatch interface.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"

#include "frame_packet.h"

/**
 * @brief Dispatch one decoded request and send its response synchronously.
 *
 * Call from task context with an initialized application link. The request must
 * remain valid and unchanged until return. This function creates no task and may
 * block inside app_link_send(); it does not impose a send timeout.
 *
 * GET_STATUS requires an empty payload. Its 16-byte little-endian response holds
 * uptime in milliseconds (uint64_t), free heap bytes (uint32_t), and historical
 * minimum free heap bytes (uint32_t), in that order. Session and sequence values
 * are copied from the request. Heap readings are not an atomic snapshot.
 *
 * @param request Required pointer to a validated, decoded request packet.
 * @return ESP_ERR_INVALID_ARG for NULL or invalid command payload;
 *         ESP_ERR_NOT_SUPPORTED for unknown types; otherwise app_link_send()'s
 *         result. Rejected requests do not send a protocol error response.
 *         ESP_OK means local driver acceptance, not remote reception.
 */
esp_err_t app_protocol_handle_packet(const frame_packet_t *request);
