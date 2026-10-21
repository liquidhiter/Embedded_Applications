idf_component_register(SRCS "test_uart_link.c"
                       PRIV_REQUIRES app_link app_uart esp_common esp_driver_uart log frame_codec)
