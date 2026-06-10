#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef enum { CLK_MODE_NTP = 0, CLK_MODE_MANUAL } clk_mode_t;

// Start the timekeeper task. Until time is valid it blinks "1" on all tubes at 1 Hz.
void timekeeper_start(void);

// Called by NTP after a successful sync (marks time valid, refreshes display).
void timekeeper_on_ntp_synced(void);

bool timekeeper_time_valid(void);

void       timekeeper_set_mode(clk_mode_t m);
clk_mode_t timekeeper_get_mode(void);

// Manual time set (HH:MM). Sets RTC, switches to MANUAL, marks time valid.
void timekeeper_set_manual_time(int hh, int mm);

// Raw test override (digits 0..9 or DISPLAY_BLANK). Suppressed by next mode/time change.
void timekeeper_set_override(int h10, int h1, int m10, int m1);
void timekeeper_clear_override(void);

// Anti-poisoning (nightly cathode cleaning). enable: compile-time default via main.
void timekeeper_set_antipoison(bool en);
bool timekeeper_get_antipoison(void);
esp_err_t timekeeper_run_antipoison(void);   // trigger the routine now (non-blocking caller)

// Wake the task to re-render immediately.
void timekeeper_notify(void);
