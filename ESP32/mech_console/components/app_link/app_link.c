/*
 * @file    app_link.c
 * @author  maxin
 * @date    2026-10-20
 * @brief   UART receive task and validated packet delivery.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_link.h"
#include "app_task_config.h"
#include "app_uart_config.h"
#include "frame_packet.h"
#include "frame_stream.h"

/*! @brief Indicates whether the application link has been started. */
static bool app_link_started = false;

/*! @brief Lifecycle of the application link's frame stream needs to be active
 *         while the application link is started.
 */
static frame_stream_t app_link_frame_stream;

static app_link_packet_callback_t app_link_callback = NULL;
static void *app_link_context = NULL;

ESP_LOG_ATTR_TAG(TAG, "APP_LINK");

static void on_encoded_frame(const uint8_t *encoded, size_t encoded_len, void *context)
{
    (void)context;

    frame_packet_t packet;
    frame_codec_result_t result;

    result = frame_packet_decode(encoded, encoded_len, &packet);
    if (result == FRAME_CODEC_OK) {
        if (app_link_callback != NULL) {
            app_link_callback(&packet, app_link_context);
        }
    }
}

static void receive_task(void *context)
{
    (void)context;

    uint8_t buffer[256];
    int received = -1;

    for (;;) {
        received = uart_read_bytes(APP_UART_NUM, buffer, sizeof(buffer), pdMS_TO_TICKS(100));
        if (received > 0) {
            /** Feed the received data into the frame stream for decoding.
             * @NOTE: frame_stream_feed always returns FRAME_CODEC_OK as app_link_frame_stream
             *        and buffer and number of received bytes are always valid.
             */
            (void)frame_stream_feed(&app_link_frame_stream, buffer, received);
        } else if (received < 0) {
            ESP_LOGE(TAG, "UART read failed; retrying");
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

esp_err_t app_link_start(app_link_packet_callback_t callback, void *context)
{
    /** Check if the application link has already been started. */
    if (app_link_started) {
        return ESP_ERR_INVALID_STATE;
    }

    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Store the callback and context for later use. */
    app_link_callback = callback;
    app_link_context = context;

    /* Initialize the frame stream for the application link. */
    frame_codec_result_t result;
    result = frame_stream_init(&app_link_frame_stream, on_encoded_frame, NULL);
    if (result != FRAME_CODEC_OK) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Create the receive task for handling incoming UART data. */
    BaseType_t task_result;
    task_result = xTaskCreate(receive_task, "app_link_receive_task", APP_LINK_RECEIVE_TASK_STACK_SIZE, NULL,
                              APP_LINK_RECEIVE_TASK_PRIORITY, NULL);
    if (task_result != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    app_link_started = true;

    return ESP_OK;
}
