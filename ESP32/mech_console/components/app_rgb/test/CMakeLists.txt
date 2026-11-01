idf_component_register(SRCS "test_app_rgb.c"
                       PRIV_REQUIRES app_rgb led_strip freertos unity
                       WHOLE_ARCHIVE)

# Other component cases and the physical RGB case use the real driver.
foreach(api led_strip_new_rmt_device led_strip_clear led_strip_del led_strip_set_pixel led_strip_refresh)
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=${api}")
endforeach()
