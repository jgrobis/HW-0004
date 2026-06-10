#pragma once

// ---- GPIO (from HW-SW ICD) ----
#define PIN_I2C_SDA        6
#define PIN_I2C_SCL        7
#define PIN_HV_EN          14   // HV driver control GPIO; actual polarity selected in web GUI/NVS

// Reserved for future sensor (LD2410S / BMP280) - not connected in this iteration
#define PIN_SENSOR_SENSE   3
#define PIN_SENSOR_RX      4
#define PIN_SENSOR_TX      5

// ---- I2C ----
#define I2C_PORT_NUM       0
#define I2C_FREQ_HZ        400000
#define TCA_ADDR_U500      0x27  // A2A1A0 = 111 -> hours side (REG1s/REG2s)
#define TCA_ADDR_U501      0x21  // A2A1A0 = 001 -> minutes side (REG3s/REG4s)

// ---- Time ----
// Europe/Warsaw, automatic CET/CEST DST
#define TZ_POLAND          "CET-1CEST,M3.5.0,M10.5.0/3"

// NTP servers
#define NTP_SERVER_PRIMARY "0.pl.pool.ntp.org"
#define NTP_SERVER_BACKUP  "tempus1.gum.gov.pl"

// ---- Behaviour ----
#define HV_ON_AT_BOOT      1    // keep HV on at boot (separator + blink "1" visible)
#define ANTIPOISON_ENABLE  1    // default: automatic cathode anti-poisoning enabled
