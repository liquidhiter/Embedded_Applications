/*
 * @file    app_task_config.h
 * @brief   Central task configuration for application-owned production tasks.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "freertos/FreeRTOS.h"

#define APP_HEALTH_TASK_PRIORITY (1U)
#define APP_LINK_RECEIVE_TASK_PRIORITY (1U)

#define APP_HEALTH_TASK_STACK_SIZE (2048U)
#define APP_LINK_RECEIVE_TASK_STACK_SIZE (4096U)

#if APP_HEALTH_TASK_PRIORITY >= configMAX_PRIORITIES
#error "APP_HEALTH_TASK_PRIORITY must be less than configMAX_PRIORITIES"
#endif

#if APP_LINK_RECEIVE_TASK_PRIORITY >= configMAX_PRIORITIES
#error "APP_LINK_RECEIVE_TASK_PRIORITY must be less than configMAX_PRIORITIES"
#endif
