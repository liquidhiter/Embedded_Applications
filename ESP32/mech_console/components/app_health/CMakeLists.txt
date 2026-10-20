idf_component_register(SRCS "app_health.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES app_config esp_timer freertos log)
