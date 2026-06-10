idf_component_register(SRCS "display.c"
                       INCLUDE_DIRS "include"
                       REQUIRES tca9535 nvs_flash
                       PRIV_REQUIRES esp_common)
