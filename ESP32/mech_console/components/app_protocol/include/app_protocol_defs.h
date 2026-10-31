/**
 * @file    app_protocol_defs.h
 * @author  maxin
 * @date    2026-10-31
 * @brief   Definitions and constants for the application protocol.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/**
 * @brief   Enumeration of application protocol packet types.
 */
typedef enum { APP_PACKET_TYPE_GET_STATUS = 0x01 } app_packet_type_e;

/**
 * @brief   Enumeration of application protocol response packet types.
 */
typedef enum { APP_PACKET_RESPONSE_STATUS = 0x81 } app_packet_response_type_e;

/**
 * @brief   Response packet payload length for each response type.
 */
#define APP_PACKET_RESPONSE_STATUS_PAYLOAD_LEN                                                                         \
    (16) /**< 8 bytes for uptime + 4 bytes for free heap + 4 bytes for min free heap */
