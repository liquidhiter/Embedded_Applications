/*
 * @file    app_health.c
 * @author  maxin
 * @date    2026-10-18
 * @brief   
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_health.h"

#define APP_HEALTH_TASK_STACK_SIZE (2048U)
#define APP_HEALTH_TASK_PRIORITY (1U)

#define MICROSECONDS_IN_SECOND (1000000U)

#define APP_HEALTH_LOG_INTERVAL_MS (1000U) /* 1 second */

/** @brief Indicates whether the app health module has been initialized or not */
static bool app_health_initialized = false;

/** @brief Indicates whether a stop request has been made for the app health task */
static bool app_health_requested_stop = false;

/** @brief Handle to the app health task */
static TaskHandle_t app_health_task_handle = NULL;

/* Application tag for logging */
ESP_LOG_ATTR_TAG(TAG, "APP_HEALTH");

/**
 * @brief   Task function for the app health module.
 *
 *          每秒输出运行时间、当前剩余堆、历史最小剩余堆
 *
 * @param   pvParameters  Pointer to the task parameters (unused).
 */
static void app_health_task(void *pvParameters);

/**
 * @brief   Handle the stop request for the app health task.
 *
 *          This function is called when the app health task receives a stop request.
 *          It performs any necessary cleanup and deletes the task.
 */
static void handle_app_health_stop_request(void);

/** @brief  Get the current execution time in seconds */
static inline uint64_t _get_current_exec_time(void)
{
    uint64_t time_us = esp_timer_get_time();
    /* Convert microseconds to seconds */
    return time_us / MICROSECONDS_IN_SECOND;
}

/** @brief Get the current free heap size */
static inline size_t _get_current_free_heap(void)
{
    return xPortGetFreeHeapSize();
}

/** @brief Get the minimum ever free heap size */
static inline size_t _get_minimum_ever_free_heap(void)
{
    return xPortGetMinimumEverFreeHeapSize();
}

static inline void _log_app_health_status(void)
{
    /** @TODO: What if the ISR preempts this function?
     *         In that case, the logged values might not accurately reflect the current state.
     *  However, this function is intended for logging purposes and minor inaccuracies due to
     *  ISR preemption are generally acceptable.
     */
    /* Get the current time */
    uint64_t exec_time = _get_current_exec_time();
    size_t current_free_heap = _get_current_free_heap();
    size_t minimum_ever_free_heap = _get_minimum_ever_free_heap();

    ESP_LOGI(TAG, "Execution Time: %llu s, Current Free Heap: %u bytes, Minimum Ever Free Heap: %u bytes", exec_time,
             current_free_heap, minimum_ever_free_heap);
}

/** @NOTE: app_health_start is only called from the app_main
 *         Omit the critical section for app_health_initialized access
 */
esp_err_t app_health_start(void)
{
    if (app_health_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Create the app health task: save the task handle */
    BaseType_t result = xTaskCreate(app_health_task, "app_health_task", APP_HEALTH_TASK_STACK_SIZE, NULL,
                                    APP_HEALTH_TASK_PRIORITY, &app_health_task_handle);
    if (result != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    app_health_initialized = true;

    ESP_LOGI(TAG, "App health task started successfully.");

    return ESP_OK;
}

esp_err_t app_health_request_stop(void)
{
    if (!app_health_initialized) {
        /* Can't stop if the task is not running */
        return ESP_ERR_INVALID_STATE;
    }

    if (app_health_requested_stop) {
        /* Stop request already sent */
        return ESP_ERR_INVALID_STATE;
    }

    /* Notify the app health task to stop: always returns pdPASS */
    (void)xTaskNotifyGive(app_health_task_handle);

    /* Save the stop request state after notifying the task */
    app_health_requested_stop = true;

    return ESP_OK;
}

static void app_health_task(void *pvParameters)
{
    (void)pvParameters;

    uint32_t notification_value;

    for (;;) {
        /* Log the current app health status */
        _log_app_health_status();

        /* Wait for the stop request or the log interval to elapse */
        notification_value = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(APP_HEALTH_LOG_INTERVAL_MS));

        if (notification_value > 0) {
            handle_app_health_stop_request();
        }
    }
}

static void handle_app_health_stop_request(void)
{
    /* Stop request received */
    ESP_LOGI(TAG, "Stop request received. Exiting app health task.");
    vTaskDelete(app_health_task_handle);
}
