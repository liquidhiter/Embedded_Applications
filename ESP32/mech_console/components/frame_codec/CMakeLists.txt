idf_component_register(SRCS "frame_codec.c" "frame_stream.c" "frame_crc.c" "frame_packet.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES app_utils)
