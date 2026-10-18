/*
 * @file    app_health.h
 * @author  maxin
 * @date    2026-10-18
 * @brief   
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"

/**
 * @brief   Start the app health monitoring task.
 *
 *          Creates a low-priority FreeRTOS task that logs, once per second,
 *          the execution time, the current free heap and the minimum ever
 *          free heap.
 *
 * @note    Not thread-safe; intended to be called once from app_main.
 *
 * @return
 *          - ESP_OK                 Task created successfully.
 *          - ESP_ERR_INVALID_STATE  Already started.
 *          - ESP_ERR_NO_MEM         Task could not be created (insufficient memory).
 */
esp_err_t app_health_start(void);

/**
 * @brief   Request the app health monitoring task to stop.
 *
 *          Signals the app health task to terminate itself gracefully.
 *
 * @note    Not thread-safe; intended to be called from app_main or a similar context.
 *
 * @return
 *          - ESP_OK                 Stop request sent successfully.
 *          - ESP_ERR_INVALID_STATE  Task not running.
 */
esp_err_t app_health_request_stop(void);