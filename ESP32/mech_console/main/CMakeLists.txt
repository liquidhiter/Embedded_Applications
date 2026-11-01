idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES app_health app_uart app_link app_protocol app_command app_rgb app_session esp_hw_support esp_system log spi_flash
                       INCLUDE_DIRS "")
