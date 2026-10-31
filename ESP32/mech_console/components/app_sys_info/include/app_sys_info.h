/*
 * @file    app_sys_info.h
 * @author  maxin
 * @date    2026-10-31
 * @brief   Shared system information helpers: uptime and heap statistics.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_timer.h"

#include "freertos/FreeRTOS.h"

#define APP_SYS_MICROSECONDS_PER_MILLISECOND (1000U)

/** @brief  Get the time since system start in milliseconds */
static inline uint64_t app_sys_get_uptime_ms(void)
{
    return (uint64_t)esp_timer_get_time() / APP_SYS_MICROSECONDS_PER_MILLISECOND;
}

/** @brief  Get the current free heap size in bytes */
static inline size_t app_sys_get_free_heap(void)
{
    return xPortGetFreeHeapSize();
}

/** @brief  Get the minimum ever free heap size in bytes */
static inline size_t app_sys_get_min_free_heap(void)
{
    return xPortGetMinimumEverFreeHeapSize();
}
