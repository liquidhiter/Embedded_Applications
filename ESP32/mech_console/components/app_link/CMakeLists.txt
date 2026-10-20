idf_component_register(SRCS "app_link.c"
                       INCLUDE_DIRS "include"
                       # Public headers include frame_packet.h and esp_err.h.
                       REQUIRES frame_codec esp_common
                       # Only the implementation uses UART configuration and task APIs.
                       PRIV_REQUIRES app_config app_uart freertos esp_driver_uart log)
