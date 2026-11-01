idf_component_register(SRCS "app_command.c"
                       INCLUDE_DIRS "include"
                       # Public headers include frame_packet.h and esp_err.h.
                       REQUIRES frame_codec esp_common
                       # The worker uses protocol dispatch and shared task configuration.
                       PRIV_REQUIRES app_config app_protocol freertos log)
