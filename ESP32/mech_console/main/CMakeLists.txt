idf_component_register(SRCS "hello_world_main.c"
                       PRIV_REQUIRES spi_flash esp_timer app_health app_uart
                       INCLUDE_DIRS "")
