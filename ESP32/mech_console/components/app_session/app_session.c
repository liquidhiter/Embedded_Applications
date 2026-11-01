/**
 * @file    app_session.c
 * @author  maxin
 * @date    2026-11-01
 * @brief   UART session handshake validation and response construction.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"

#include "app_session.h"
#include "app_utils.h"
#include "frame_packet.h"

/* Application tag for logging */
ESP_LOG_ATTR_TAG(TAG, "APP_SESSION");

/** @brief Validated HELLO parameters; features are fixed to zero. */
typedef struct {
    uint64_t host_nonce;
    uint16_t requested_window;
    uint16_t max_payload;
} hello_params_t;

/** @brief Parameters extracted from a CONFIRM request. */
typedef struct {
    uint64_t session_id;
    uint64_t host_nonce;
} confirm_params_t;

static esp_err_t parse_hello(const frame_packet_t *request, hello_params_t *params);
static void build_hello_ack(const app_session_t *session, frame_packet_t *response);
static esp_err_t parse_confirm(const frame_packet_t *request, confirm_params_t *params);
static void build_confirm_ack(const app_session_t *session, frame_packet_t *response);

esp_err_t app_session_init(app_session_t *session, uint64_t device_boot_id, app_session_generate_id_t generate_id,
                           void *context)
{
    /* Validate input arguments */
    if (session == NULL) {
        ESP_LOGE(TAG, "Invalid argument: session is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (device_boot_id == 0) {
        ESP_LOGE(TAG, "Invalid argument: device_boot_id is zero");
        return ESP_ERR_INVALID_ARG;
    }

    if (generate_id == NULL) {
        ESP_LOGE(TAG, "Invalid argument: generate_id is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* Initialize session state */
    session->state = APP_SESSION_DOWN;
    session->device_boot_id = device_boot_id;
    session->session_id = 0;
    session->host_nonce = 0;
    session->accepted_window = 0;
    session->max_payload = 0;
    session->generate_id = generate_id;
    session->context = context;

    return ESP_OK;
}

esp_err_t app_session_handle_hello(app_session_t *session, const frame_packet_t *request, frame_packet_t *response)
{
    if (session == NULL || request == NULL || response == NULL) {
        ESP_LOGE(TAG, "Invalid argument: session, request and response are required");
        return ESP_ERR_INVALID_ARG;
    }

    hello_params_t hello;
    esp_err_t err = parse_hello(request, &hello);
    if (err != ESP_OK) {
        return err;
    }

    /* Prepare the next state locally; failures leave caller storage unchanged. */
    app_session_t next = *session;
    switch (next.state) {
        case APP_SESSION_DOWN:
            next.session_id = next.generate_id(next.context);
            if (next.session_id == 0) {
                ESP_LOGE(TAG, "Generated session ID is zero");
                return ESP_FAIL;
            }
            next.host_nonce = hello.host_nonce;
            next.accepted_window = hello.requested_window;
            next.max_payload = hello.max_payload;
            next.state = APP_SESSION_HANDSHAKE;
            break;

        case APP_SESSION_HANDSHAKE:
            if (hello.host_nonce != next.host_nonce || hello.requested_window != next.accepted_window ||
                hello.max_payload != next.max_payload) {
                ESP_LOGE(TAG, "HELLO conflicts with the pending handshake");
                return ESP_ERR_INVALID_STATE;
            }
            /* Duplicate HELLO: reconstruct the ACK using the existing session ID. */
            break;

        default:
            ESP_LOGE(TAG, "Cannot handle HELLO in the current session state");
            return ESP_ERR_INVALID_STATE;
    }

    /* All operations are complete. Sending the ACK is the caller's job. */
    build_hello_ack(&next, response);
    *session = next;
    return ESP_OK;
}

esp_err_t app_session_handle_confirm(app_session_t *session, const frame_packet_t *request, frame_packet_t *response)
{
    if (session == NULL || request == NULL || response == NULL) {
        ESP_LOGE(TAG, "Invalid argument: session, request and response are required");
        return ESP_ERR_INVALID_ARG;
    }

    confirm_params_t confirm;
    esp_err_t err = parse_confirm(request, &confirm);
    if (err != ESP_OK) {
        return err;
    }

    /* Prepare the next state locally; failures leave caller storage unchanged. */
    app_session_t next = *session;
    switch (next.state) {
        case APP_SESSION_HANDSHAKE:
            /* Validate the session ID and host nonce against the pending handshake. */
            if (confirm.session_id != next.session_id || confirm.host_nonce != next.host_nonce) {
                ESP_LOGE(TAG, "CONFIRM does not match the pending handshake");
                return ESP_ERR_INVALID_STATE;
            }
            next.state = APP_SESSION_ACTIVE;
            break;
        case APP_SESSION_ACTIVE:
            /* Validate the session ID and host nonce against the accepted session. */
            if (confirm.session_id != next.session_id || confirm.host_nonce != next.host_nonce) {
                ESP_LOGE(TAG, "CONFIRM does not match the pending handshake");
                return ESP_ERR_INVALID_STATE;
            }
            /* Duplicate CONFIRM: reconstruct READY using the existing session ID. */
            break;
        default:
            ESP_LOGE(TAG, "Cannot handle CONFIRM in the current session state");
            return ESP_ERR_INVALID_STATE;
    }
    /* All operations are complete. Sending READY is the caller's job. */
    build_confirm_ack(&next, response);
    *session = next;
    return ESP_OK;
}

static esp_err_t parse_hello(const frame_packet_t *request, hello_params_t *params)
{
    /* The public entry point checks pointers before calling this helper. */
    if (request->type != APP_SESSION_PACKET_HELLO || request->session_id != 0 || request->sequence != 0 ||
        request->payload_length != APP_SESSION_HELLO_PAYLOAD_LEN) {
        ESP_LOGE(TAG, "Invalid HELLO header");
        return ESP_ERR_INVALID_ARG;
    }

    hello_params_t parsed = {
        .host_nonce = app_read_u64_le(request->payload),
        .requested_window = app_read_u16_le(request->payload + 8),
        .max_payload = app_read_u16_le(request->payload + 10),
    };
    uint32_t features = app_read_u32_le(request->payload + 12);
    if (parsed.requested_window < 1 || parsed.requested_window > APP_SESSION_MAX_WINDOW) {
        ESP_LOGE(TAG, "HELLO requested_window out of range");
        return ESP_ERR_INVALID_ARG;
    }
    if (parsed.max_payload < 1 || parsed.max_payload > FRAME_PACKET_MAX_PAYLOAD_SIZE) {
        ESP_LOGE(TAG, "HELLO max_payload out of range");
        return ESP_ERR_INVALID_ARG;
    }
    if (features != 0) {
        ESP_LOGE(TAG, "HELLO features must be zero");
        return ESP_ERR_INVALID_ARG;
    }

    *params = parsed;
    return ESP_OK;
}

static void build_hello_ack(const app_session_t *session, frame_packet_t *response)
{
    *response = (frame_packet_t){
        .type = APP_SESSION_PACKET_HELLO_ACK,
        .session_id = session->session_id,
        .sequence = 0,
        .payload_length = APP_SESSION_HELLO_ACK_PAYLOAD_LEN,
    };
    app_write_u64_le(response->payload, session->host_nonce);
    app_write_u64_le(response->payload + 8, session->device_boot_id);
    app_write_u16_le(response->payload + 16, session->accepted_window);
    app_write_u16_le(response->payload + 18, session->max_payload);
    app_write_u32_le(response->payload + 20, 0);
}

static esp_err_t parse_confirm(const frame_packet_t *request, confirm_params_t *params)
{
    /* The public entry point checks pointers before calling this helper. */
    if (request->type != APP_SESSION_PACKET_CONFIRM || request->session_id == 0 || request->sequence != 0 ||
        request->payload_length != APP_SESSION_CONFIRM_PAYLOAD_LEN) {
        ESP_LOGE(TAG, "Invalid CONFIRM header");
        return ESP_ERR_INVALID_ARG;
    }

    confirm_params_t parsed = {
        .session_id = request->session_id,
        .host_nonce = app_read_u64_le(request->payload),
    };

    *params = parsed;
    return ESP_OK;
}

static void build_confirm_ack(const app_session_t *session, frame_packet_t *response)
{
    *response = (frame_packet_t){
        .type = APP_SESSION_PACKET_READY,
        .session_id = session->session_id,
        .sequence = 0,
        .payload_length = APP_SESSION_READY_PAYLOAD_LEN,
    };
}
