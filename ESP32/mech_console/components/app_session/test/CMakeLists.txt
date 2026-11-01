idf_component_register(SRCS "test_app_session.c"
                       PRIV_REQUIRES app_session app_utils frame_codec unity
                       WHOLE_ARCHIVE)
