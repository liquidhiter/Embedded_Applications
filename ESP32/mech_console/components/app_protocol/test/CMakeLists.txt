idf_component_register(SRCS "test_app_protocol.c"
                       PRIV_REQUIRES app_protocol app_link app_utils esp_timer freertos unity
                       WHOLE_ARCHIVE)
# Inactive wrappers delegate to the real APIs for other component cases.
target_link_options(${COMPONENT_LIB} INTERFACE
                    "-Wl,--wrap=app_link_send"
                    "-Wl,--wrap=esp_timer_get_time"
                    "-Wl,--wrap=xPortGetFreeHeapSize"
                    "-Wl,--wrap=xPortGetMinimumEverFreeHeapSize")
