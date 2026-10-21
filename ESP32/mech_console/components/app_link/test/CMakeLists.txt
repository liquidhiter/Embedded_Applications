idf_component_register(SRCS "test_app_link.c"
                       PRIV_REQUIRES app_link app_uart esp_driver_uart freertos frame_codec unity
                       WHOLE_ARCHIVE)
# Replace UART1 I/O in this test image; keep the real console UART intact.
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=uart_read_bytes" "-Wl,--wrap=uart_write_bytes")
