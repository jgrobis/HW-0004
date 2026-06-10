#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Initialise SNTP (two servers) and start the scheduler task.
// Syncs at boot (once WiFi is up), then every configurable interval.
void ntp_sync_start(const char *primary, const char *backup);

// Force an immediate sync (e.g. from the web UI).
void ntp_sync_now(void);

// Configurable sync interval. Default is 720 min (12 h), saved in NVS.
uint32_t  ntp_sync_get_interval_minutes(void);
esp_err_t ntp_sync_set_interval_minutes(uint32_t minutes, bool save);
