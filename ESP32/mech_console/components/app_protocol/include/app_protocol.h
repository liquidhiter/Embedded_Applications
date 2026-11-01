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
 * COMMAND payload: request_id u32, opcode u16, args_length u16, args (0..56).
 * RESPONSE payload: request_id u32, status u16, result_length u16, result.
 * All integers are little-endian. Control sequence must be zero; session_id is
 * echoed without session validation. Request-ID ordering/deduplication is pending.
 * GET_STATUS (opcode 1) takes no args and returns uptime_ms u64, free_heap u32,
 * min_heap u32. Heap readings are not an atomic snapshot.
 *
 * @param request Required pointer to a validated, decoded request packet.
 * @return ESP_ERR_INVALID_ARG for NULL, invalid envelope or nonzero sequence;
 *         ESP_ERR_NOT_SUPPORTED for other packet types; otherwise the send result.
 *         Invalid envelopes and unsupported packet types send no response.
 *         Valid envelopes get OK, INVALID (GET_STATUS args) or UNSUPPORTED status.
 *         ESP_OK means local driver acceptance, not remote reception or command
 *         success; the RESPONSE status describes the command result.
 */
esp_err_t app_protocol_handle_packet(const frame_packet_t *request);
