idf_component_register(SRCS "app_session.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common frame_codec
                       PRIV_REQUIRES app_utils log)
