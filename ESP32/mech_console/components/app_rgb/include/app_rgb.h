/**
 * @file    app_rgb.h
 * @author  maxin
 * @date    2026-11-01
 * @brief   Board RGB LED interface.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

/**
 * @brief Initialize the LED and turn it off.
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if already initialized or
 *         cleanup previously failed, or the original creation/clear error.
 * @note Retry is allowed after a failed initialization only if cleanup succeeded.
 *       A cleanup failure requires a reboot. Calls must be serialized by the caller.
 */
esp_err_t app_rgb_init(void);

/**
 * @brief Set the board LED color and transmit it synchronously.
 * @param red Red intensity, 0..255.
 * @param green Green intensity, 0..255.
 * @param blue Blue intensity, 0..255.
 * @return ESP_ERR_INVALID_STATE before successful initialization; otherwise the
 *         set-pixel or refresh result. Calls must be serialized by the caller.
 */
esp_err_t app_rgb_set(uint8_t red, uint8_t green, uint8_t blue);
