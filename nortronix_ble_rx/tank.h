// tank.h — esp32-Nortronix-ble: receive tank beacon on the display (NimBLE-Arduino 2.x)
#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

struct TankState {
  uint16_t fv_mv = 0;
  uint16_t gv_mv = 0;
  uint8_t  seq = 0;
  uint32_t last_seen_ms = 0;     // 0 = never seen
};

// Defined in tank.cpp — declare only here (avoids multiple-definition link errors)
extern TankState    g_tank;
extern portMUX_TYPE g_tank_mux;

// Call first in your onResult(); returns true if the advert was ours.
bool tank_parse(const NimBLEAdvertisedDevice* dev);

// Thread-safe copy for the LVGL/UI task
TankState tank_snapshot();

// mV -> 0..100 %. Works also if full_mv < empty_mv (inverted sensor).
// Returns -1 if no data, stale (> stale_ms), or signal below min_valid_mv
// (panel/box unpowered — show "--" instead of "0 %").
int tank_percent(uint16_t mv, uint32_t last_seen_ms,
                 uint16_t empty_mv, uint16_t full_mv,
                 uint32_t stale_ms = 60000, uint16_t min_valid_mv = 100);
