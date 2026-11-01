/**
 * @file    app_session.h
 * @author  maxin
 * @date    2026-11-01
 * @brief   Caller-owned UART session state and handshake interface.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#include "frame_packet.h"

#define APP_SESSION_HELLO_PAYLOAD_LEN (16U)
#define APP_SESSION_HELLO_ACK_PAYLOAD_LEN (24U)
#define APP_SESSION_CONFIRM_PAYLOAD_LEN (8U)
#define APP_SESSION_READY_PAYLOAD_LEN (0U)
#define APP_SESSION_MAX_WINDOW (8U)

typedef enum {
    APP_SESSION_PACKET_HELLO = 0x01,
    APP_SESSION_PACKET_HELLO_ACK = 0x02,
    APP_SESSION_PACKET_CONFIRM = 0x03,
    APP_SESSION_PACKET_READY = 0x04,
} app_session_packet_type_t;

typedef enum {
    APP_SESSION_DOWN = 0,
    APP_SESSION_HANDSHAKE,
    APP_SESSION_ACTIVE, /* Matching CONFIRM accepted; peer receipt of READY is not implied. */
} app_session_state_t;

/** @brief Return a fresh nonzero session ID. Randomness is supplied by the caller. */
typedef uint64_t (*app_session_generate_id_t)(void *context);

/**
 * @brief Caller-owned state for one UART peer's handshake.
 *
 * Initialize through app_session_init() and keep the instance alive while it is
 * in use. The module updates these fields; callers must not modify them directly.
 * Use one task as the owner; the structure provides no internal synchronization.
 * This is an in-memory representation, not a wire-format structure.
 *
 * Boot identity and callback configuration remain valid in DOWN. A successful
 * HELLO fills the pending-session fields and enters HANDSHAKE. Those fields are
 * retained for retries and CONFIRM validation. A matching CONFIRM enters ACTIVE;
 * matching duplicate CONFIRM packets reconstruct the original READY.
 */
typedef struct {
    /** @brief Current lifecycle state; initialized to APP_SESSION_DOWN. */
    app_session_state_t state;

    /** @brief Nonzero device identity for this boot, shared across its sessions. */
    uint64_t device_boot_id;

    /** @brief Device-generated nonzero pending/active session ID; zero after init. */
    uint64_t session_id;

    /** @brief Accepted HELLO nonce, retained for CONFIRM and retry validation. */
    uint64_t host_nonce;

    /** @brief Accepted DATA window in frames, 1..8; zero after init. Not credit by itself. */
    uint16_t accepted_window;

    /** @brief Negotiated maximum payload in bytes, 1..512; zero after init. */
    uint16_t max_payload;

    /** @brief Required ID generator, called for a new valid HELLO, never for a duplicate. */
    app_session_generate_id_t generate_id;

    /** @brief Optional borrowed generator context; valid for the instance lifetime. */
    void *context;
} app_session_t;

/**
 * @brief Initialize one instance in DOWN before traffic starts.
 * @param device_boot_id Nonzero boot identifier, generated once per device boot.
 * @param generate_id Required callback, called only for a new valid HELLO in DOWN.
 * @param context Optional callback context; must outlive the instance.
 * @return ESP_ERR_INVALID_ARG for NULL session/callback or zero boot ID; ESP_OK otherwise.
 * @note No task, UART access, dynamic allocation or synchronization. All calls on
 *       an instance must use one owner. Do not reinitialize an active instance.
 */
esp_err_t app_session_init(app_session_t *session, uint64_t device_boot_id, app_session_generate_id_t generate_id,
                           void *context);

/**
 * @brief Validate HELLO and construct HELLO_ACK without transmitting it.
 *
 * All payload integers are little-endian:
 * HELLO: host_nonce u64, requested_window u16, max_payload u16, features u32.
 * ACK: host_nonce u64, device_boot_id u64, accepted_window u16,
 *      max_payload u16, features u32.
 *
 * @verbatim
 * HELLO_ACK decoded packet:
 * +--------------------+--------------------+------------------+
 * | Header: 20 bytes   | Payload: 24 bytes  | CRC32: 4 bytes   |
 * +--------------------+--------------------+------------------+
 * 0                   20                   44                 48
 *
 * Header: version=1, type=0x02, flags=0, session_id=new nonzero ID,
 *         sequence=0, payload_length=24, reserved=0.
 *
 * Payload offsets (relative to the payload start):
 * +--------+-----------------+------+---------------------------+
 * | Offset | Field           | Size | Source                    |
 * +--------+-----------------+------+---------------------------+
 * |  0..7  | host_nonce      | u64  | Accepted HELLO            |
 * |  8..15 | device_boot_id  | u64  | Device boot identity      |
 * | 16..17 | accepted_window | u16  | Accepted window, 1..8     |
 * | 18..19 | max_payload     | u16  | Accepted limit, 1..512 B  |
 * | 20..23 | features        | u32  | Zero for the baseline     |
 * +--------+-----------------+------+---------------------------+
 * @endverbatim
 *
 * The handler constructs a frame_packet_t, not encoded wire bytes. The packet
 * encoder adds CRC32 over header/payload (bytes 0..43), applies COBS and appends
 * the 0x00 delimiter. Header version/flags/reserved are supplied by the encoder.
 *
 * Require HELLO type, header session/sequence zero, exact 16-byte payload,
 * window 1..8, max_payload 1..512 and features zero (baseline only).
 * In DOWN, generate a nonzero ID and enter HANDSHAKE. Accept the requested
 * window/payload limits; ACK uses that session ID and sequence zero.
 * In HANDSHAKE, an identical HELLO reconstructs the same ACK without generating
 * another ID. Changed nonce/parameters or ACTIVE state return INVALID_STATE.
 * Retain pending state if the caller subsequently fails to send the ACK.
 *
 * @return ESP_ERR_INVALID_ARG for NULL arguments or malformed HELLO;
 *         ESP_ERR_INVALID_STATE for a conflicting handshake/state;
 *         ESP_FAIL if the ID generator returns zero; ESP_OK with a complete ACK.
 * @note Call init first. Input/output/instance storage must not overlap. Errors
 *       leave the response and instance unchanged. HELLO handling does not provide timeouts or credit.
 */
esp_err_t app_session_handle_hello(app_session_t *session, const frame_packet_t *request, frame_packet_t *response);

/**
 * @brief Validate CONFIRM and prepare READY without transmitting it.
 *
 * @verbatim
 * CONFIRM decoded packet:
 * +--------------------+--------------------+------------------+
 * | Header: 20 bytes   | host_nonce: 8 B    | CRC32: 4 bytes   |
 * +--------------------+--------------------+------------------+
 * 0                   20                   28                 32
 * Header: type=0x03, session_id=accepted ID, sequence=0, payload_length=8.
 * Payload: accepted host_nonce, u64 little-endian at payload offset 0.
 *
 * READY decoded packet (empty payload):
 * +--------------------+------------------+
 * | Header: 20 bytes   | CRC32: 4 bytes   |
 * +--------------------+------------------+
 * 0                   20                 24
 * Header: type=0x04, session_id=accepted ID, sequence=0, payload_length=0.
 * @endverbatim
 *
 * First validate type, exact payload length, nonzero header session ID and zero
 * sequence. Then require HANDSHAKE or ACTIVE and a matching session ID/nonce.
 * In HANDSHAKE, prepare READY and enter ACTIVE. In ACTIVE, a matching duplicate
 * CONFIRM prepares the same READY without changing session metadata. Never call
 * the ID generator or change negotiation parameters here.
 *
 * The caller sends READY separately. Keep ACTIVE if sending fails: a duplicate
 * CONFIRM must still succeed. ACTIVE means local confirmation acceptance, not
 * peer receipt of READY. READY does not grant DATA credit.
 *
 * @param session Initialized, caller-owned session instance.
 * @param request Decoded CONFIRM packet, excluding CRC/COBS/delimiter.
 * @param response Output READY packet; meaningful only on ESP_OK.
 * @return ESP_ERR_INVALID_ARG for NULL arguments, wrong type, nonzero sequence,
 *         zero header session ID or a payload length other than 8;
 *         ESP_ERR_INVALID_STATE for DOWN/unsupported state or mismatched ID/nonce;
 *         ESP_OK when READY is prepared and the session is ACTIVE.
 * @note Single owner; input/output/instance storage must not overlap. Errors
 *       leave the instance and response unchanged. No UART access or allocation.
 */
esp_err_t app_session_handle_confirm(app_session_t *session, const frame_packet_t *request, frame_packet_t *response);
