idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_i2c nvs_flash esp_pm
                                tca9535 display hv timekeeper wifi ntp_sync webserver)
