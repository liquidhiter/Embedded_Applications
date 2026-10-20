idf_component_register(SRCS "test_app_health.c"
                       PRIV_REQUIRES app_health esp_timer freertos unity
                       WHOLE_ARCHIVE)
