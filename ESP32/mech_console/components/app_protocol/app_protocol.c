/**
 * @file    app_protocol.c
 * @author  maxin
 * @date    2026-10-31
 * @brief   Command-envelope validation and status responses.
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>

#include "esp_err.h"

#include "app_link.h"
#include "app_protocol.h"
#include "app_protocol_defs.h"
#include "app_sys_info.h"
#include "app_utils.h"
#include "frame_packet.h"

esp_err_t app_protocol_handle_packet(const frame_packet_t *request)
{
    if (request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (request->type != APP_PACKET_TYPE_COMMAND) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    /* Validate the envelope before reading fields. Control sequence is zero. */
    if (request->sequence != 0 || request->payload_length < APP_PROTOCOL_ENVELOPE_LEN ||
        request->payload_length > APP_PROTOCOL_ENVELOPE_LEN + APP_PROTOCOL_MAX_ARGS_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t args_length = app_read_u16_le(request->payload + 6);
    if (args_length != request->payload_length - APP_PROTOCOL_ENVELOPE_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t request_id = app_read_u32_le(request->payload);
    uint16_t opcode = app_read_u16_le(request->payload + 4);
    frame_packet_t response = {0};
    response.type = APP_PACKET_TYPE_RESPONSE;
    response.session_id = request->session_id;
    response.payload_length = APP_PROTOCOL_ENVELOPE_LEN;
    app_write_u32_le(response.payload, request_id);

    app_response_status_e status;
    uint16_t result_length = 0;
    if (opcode != APP_COMMAND_GET_STATUS) {
        status = APP_RESPONSE_UNSUPPORTED;
    } else if (args_length != 0) {
        status = APP_RESPONSE_INVALID;
    } else {
        status = APP_RESPONSE_OK;
        result_length = APP_PROTOCOL_STATUS_RESULT_LEN;
        uint8_t *result = response.payload + APP_PROTOCOL_ENVELOPE_LEN;
        app_write_u64_le(result, app_sys_get_uptime_ms());
        app_write_u32_le(result + 8, app_sys_get_free_heap());
        app_write_u32_le(result + 12, app_sys_get_min_free_heap());
    }
    app_write_u16_le(response.payload + 4, status);
    app_write_u16_le(response.payload + 6, result_length);
    response.payload_length += result_length;
    return app_link_send(&response);
}
