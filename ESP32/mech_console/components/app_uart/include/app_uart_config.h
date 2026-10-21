/*
 * @file    app_uart_config.h
 * @author  maxin
 * @date    2026-10-21
 * @brief   Configuration header for the application UART driver.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <assert.h>

#include "driver/uart.h"

/* Shared configuration for the application driver and dedicated test firmware. */
#define APP_UART_NUM UART_NUM_1
#define APP_UART_BAUD_RATE 115200U
#define APP_UART_DATA_BITS UART_DATA_8_BITS
#define APP_UART_PARITY UART_PARITY_DISABLE
#define APP_UART_STOP_BITS UART_STOP_BITS_1
#define APP_UART_FLOW_CTRL UART_HW_FLOWCTRL_DISABLE
#define APP_UART_RX_BUF_SIZE 2048U
#define APP_UART_TX_BUF_SIZE 2048U
#define APP_UART_TX_PIN 21U
#define APP_UART_RX_PIN 20U

/* Data bits per word, mapped from the driver enum by name rather than by numeric value. */
#define APP_UART_DATA_BIT_COUNT                                                                                        \
    (APP_UART_DATA_BITS == UART_DATA_5_BITS   ? 5U                                                                     \
     : APP_UART_DATA_BITS == UART_DATA_6_BITS ? 6U                                                                     \
     : APP_UART_DATA_BITS == UART_DATA_7_BITS ? 7U                                                                     \
     : APP_UART_DATA_BITS == UART_DATA_8_BITS ? 8U                                                                     \
                                              : 0U)

_Static_assert(APP_UART_DATA_BIT_COUNT != 0U, "Unsupported APP_UART_DATA_BITS");
_Static_assert(APP_UART_STOP_BITS == UART_STOP_BITS_1, "APP_UART_TRANSMIT_TIME assumes 1 stop bit");

/* Bits on the wire per byte: start + data + parity + stop (1 bit). */
#define APP_UART_BITS_PER_BYTE (1U + APP_UART_DATA_BIT_COUNT + (APP_UART_PARITY != UART_PARITY_DISABLE) + 1U)

/* Helper macro to calculate UART transmit time in milliseconds for a given number of bytes
 * based on the UART configuration defined above. Rounded up to a whole millisecond.
 */
#define APP_UART_TRANSMIT_TIME_MS(bytes)                                                                               \
    (((uint64_t)(bytes) * APP_UART_BITS_PER_BYTE * 1000U + APP_UART_BAUD_RATE - 1U) / APP_UART_BAUD_RATE)
