idf_component_register(SRCS "app_uart.c"
                       INCLUDE_DIRS "include"
                       # app_uart_config.h exposes UART driver types and constants.
                       REQUIRES esp_common esp_driver_uart
                       PRIV_REQUIRES log)
