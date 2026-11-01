idf_component_register(SRCS "app_rgb.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES app_config log led_strip)
