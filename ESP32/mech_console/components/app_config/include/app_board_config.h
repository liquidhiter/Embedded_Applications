/**
 * @file    app_board_config.h
 * @author  maxin
 * @date    2026-11-01
 * @brief   ESP32-C3-LCDkit board configuration.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/** @brief GPIO connected to the board WS2812 data input. */
#define APP_BOARD_RGB_PIN (8U)

/** @brief Number of addressable LED packages, not color channels. */
#define APP_BOARD_RGB_LED_COUNT (1U)

/** @brief RMT counter frequency in Hz; 10 MHz gives 100 ns per tick. */
#define APP_BOARD_RGB_RMT_RESOLUTION_HZ (10000000U)
