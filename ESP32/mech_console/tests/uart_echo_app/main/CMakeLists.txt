idf_component_register(SRCS "test_uart_echo.c"
                       PRIV_REQUIRES app_uart esp_common esp_driver_uart freertos log)
