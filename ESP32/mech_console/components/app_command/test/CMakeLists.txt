idf_component_register(SRCS "test_app_command.c"
                       PRIV_REQUIRES app_command app_protocol freertos unity
                       WHOLE_ARCHIVE)
target_link_options(${COMPONENT_LIB} INTERFACE
                    "-Wl,--wrap=app_protocol_handle_packet"
                    "-Wl,--wrap=xQueueGenericCreate"
                    "-Wl,--wrap=xTaskCreatePinnedToCore"
                    "-Wl,--wrap=vQueueDelete")
