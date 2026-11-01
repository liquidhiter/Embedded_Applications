/**
 * @file    app_command.c
 * @author  maxin
 * @date    2026-11-01
 * @brief   Bounded command queue and protocol worker task.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app_command.h"
#include "app_protocol.h"
#include "app_task_config.h"
#include "frame_packet.h"

/*! @brief Indicates whether the app command module has been started */
static bool is_app_command_started = false;

/*! @brief Handle for the command queue */
static QueueHandle_t command_queue = NULL;

/*! @brief Handle for the command task */
static TaskHandle_t command_task = NULL;

/*! @brief Depth of the command queue: maximum 4 command packets */
#define APP_COMMAND_QUEUE_DEPTH (4U)

/* Application tag for logging */
ESP_LOG_ATTR_TAG(TAG, "APP_COMMAND");

/** @brief Wait for queued requests and process them in FIFO order. */
static void app_command_task(void *pvParameters);

esp_err_t app_command_start(void)
{
    if (is_app_command_started) {
        ESP_LOGW(TAG, "App command module already started.");
        return ESP_ERR_INVALID_STATE;
    }

    /* Create the command queue */
    command_queue = xQueueCreate(APP_COMMAND_QUEUE_DEPTH, sizeof(frame_packet_t));
    if (command_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue.");
        return ESP_ERR_NO_MEM;
    }

    /* Create the task for handling commands */
    BaseType_t result = xTaskCreate(app_command_task, "command_task", APP_COMMAND_TASK_STACK_SIZE, NULL,
                                    APP_COMMAND_TASK_PRIORITY, &command_task);
    if (result != pdPASS) {
        /* Release the command queue if task creation fails */
        if (command_queue != NULL) {
            vQueueDelete(command_queue);
            command_queue = NULL;
        }

        ESP_LOGE(TAG, "Failed to create command task.");
        return ESP_ERR_NO_MEM;
    }

    is_app_command_started = true;
    return ESP_OK;
}

esp_err_t app_command_submit(const frame_packet_t *request)
{
    if (!is_app_command_started) {
        ESP_LOGW(TAG, "App command module not started.");
        return ESP_ERR_INVALID_STATE;
    }

    if (request == NULL || request->payload_length > FRAME_PACKET_MAX_PAYLOAD_SIZE) {
        ESP_LOGW(TAG, "Invalid request packet.");
        return ESP_ERR_INVALID_ARG;
    }

    /* Submit the request to the command queue */
    if (xQueueSend(command_queue, request, 0) != pdPASS) {
        ESP_LOGW(TAG, "Failed to submit request to command queue.");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

static void app_command_task(void *pvParameters)
{
    (void)pvParameters;

    /* Block while the queue is empty; execute accepted requests in FIFO order. */
    frame_packet_t received_packet;
    esp_err_t err;

    for (;;) {
        if (xQueueReceive(command_queue, &received_packet, portMAX_DELAY) == pdPASS) {
            /* Process the received command packet */
            err = app_protocol_handle_packet(&received_packet);
            if (err != ESP_OK) {
                if (err == ESP_ERR_NOT_SUPPORTED || err == ESP_ERR_INVALID_ARG) {
                    ESP_LOGW(TAG, "Rejected packet: %s", esp_err_to_name(err));
                } else {
                    ESP_LOGE(TAG, "Failed to handle packet: %s", esp_err_to_name(err));
                }
            }
        }
    }
}
