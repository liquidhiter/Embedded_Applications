idf_component_register(SRCS "test_app_health.c"
                       REQUIRES app_health esp_timer unity
                       WHOLE_ARCHIVE)
