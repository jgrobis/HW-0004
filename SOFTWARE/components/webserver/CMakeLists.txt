idf_component_register(SRCS "webserver.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_server app_update esp_app_format esp_system
                                timekeeper display hv wifi ntp_sync nvs_flash)
