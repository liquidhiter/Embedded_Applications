#pragma once

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
