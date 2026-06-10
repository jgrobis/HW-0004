idf_component_register(SRCS "hv.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio esp_timer nvs_flash)
