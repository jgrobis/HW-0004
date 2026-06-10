#pragma once
#include "esp_err.h"

// Start the test/control HTTP server (LAN, no auth).
esp_err_t webserver_start(void);
