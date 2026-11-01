/**
 * @file    app_protocol_defs.h
 * @author  maxin
 * @date    2026-10-31
 * @brief   Command and response wire constants.
 * SPDX-License-Identifier: MIT
 */
#pragma once

/** @brief Control-message types; 0x01 is reserved for the future HELLO. */
typedef enum {
    APP_PACKET_TYPE_COMMAND = 0x0A,
    APP_PACKET_TYPE_RESPONSE = 0x0B,
} app_packet_type_e;

/** @brief Opcodes carried inside a COMMAND payload. */
typedef enum { APP_COMMAND_GET_STATUS = 0x0001 } app_command_opcode_e;

/** @brief Wire status values, independent of ESP-IDF error codes. */
typedef enum {
    APP_RESPONSE_OK = 0,
    APP_RESPONSE_INVALID = 1,
    APP_RESPONSE_UNSUPPORTED = 4,
} app_response_status_e;

/* COMMAND: request_id u32, opcode u16, args_length u16, args.
 * RESPONSE: request_id u32, status u16, result_length u16, result.
 */
#define APP_PROTOCOL_ENVELOPE_LEN (8U)
#define APP_PROTOCOL_MAX_ARGS_LEN (56U)
#define APP_PROTOCOL_STATUS_RESULT_LEN (16U)
