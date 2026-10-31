# Header-only system information helpers (uptime, heap) shared by application components.
idf_component_register(INCLUDE_DIRS "include"
                       REQUIRES esp_timer freertos)
