idf_component_register(SRCS "app_protocol.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common frame_codec
                       PRIV_REQUIRES app_link app_sys_info app_utils)
