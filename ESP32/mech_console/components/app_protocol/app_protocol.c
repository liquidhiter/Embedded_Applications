/**
 * @file    app_protocol.c
 * @author  maxin
 * @date    2026-10-31
 * @brief   Implementation of the application protocol functions.
 *
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

/*! @brief Handler for each incoming application protocol packet */
typedef esp_err_t (*app_protocol_packet_handler_t)(const frame_packet_t *request);

/*! @brief Handler for the GET_STATUS packet type */
static esp_err_t handle_get_status(const frame_packet_t *request);

/*! @brief Packet type indexed array of packet handlers */
static const app_protocol_packet_handler_t packet_handlers[] = {
    [APP_PACKET_TYPE_GET_STATUS] = handle_get_status,
};

/*! @brief Total number of packet handlers available */
static const size_t num_packet_handlers = sizeof(packet_handlers) / sizeof(packet_handlers[0]);

esp_err_t app_protocol_handle_packet(const frame_packet_t *request)
{
    if (request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Dispatch the packet to the appropriate handler based on its type */
    if (request->type >= num_packet_handlers || packet_handlers[request->type] == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    /* Call the handler for the packet type */
    return packet_handlers[request->type](request);
}

static esp_err_t handle_get_status(const frame_packet_t *request)
{
    /* GET_STATUS payload must be empty */
    if (request->payload_length != 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Prepare the response packet with the system status info
     * [0-7] Time since system start in milliseconds
     * [8-11] Current usable heap in bytes
     * [12-15] Historical minimum heap in bytes
     */
    frame_packet_t response = {0};
    response.type = APP_PACKET_RESPONSE_STATUS;
    response.payload_length = APP_PACKET_RESPONSE_STATUS_PAYLOAD_LEN;
    response.session_id = request->session_id;
    response.sequence = request->sequence;

    uint64_t uptime = app_sys_get_uptime_ms();
    uint32_t free_heap = app_sys_get_free_heap();
    uint32_t min_free_heap = app_sys_get_min_free_heap();

    app_write_u64_le(response.payload, uptime);
    app_write_u32_le(response.payload + 8, free_heap);
    app_write_u32_le(response.payload + 12, min_free_heap);

    esp_err_t send_status = app_link_send(&response);

    return send_status;
}
