idf_component_register(SRCS "test_frame_cobs.c" "test_frame_stream.c" "test_frame_crc.c" "test_frame_packet.c"
                       PRIV_REQUIRES unity frame_codec
                       WHOLE_ARCHIVE)
