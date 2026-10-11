// tank.cpp — esp32-Nortronix-ble: receive tank beacon on the display (NimBLE-Arduino 2.x)
#include "tank.h"
#include <string.h>

TankState    g_tank;
portMUX_TYPE g_tank_mux = portMUX_INITIALIZER_UNLOCKED;

struct __attribute__((packed)) TankAdv {   // must match transmitter
  uint16_t company;
  uint8_t  magic[2];
  uint8_t  version;
  uint8_t  seq;
  uint16_t fv_mv;
  uint16_t gv_mv;
};

bool tank_parse(const NimBLEAdvertisedDevice* dev) {
  if (!dev->haveManufacturerData()) return false;
  std::string md = dev->getManufacturerData();
  if (md.size() < sizeof(TankAdv)) return false;

  TankAdv p;
  memcpy(&p, md.data(), sizeof(p));        // no unaligned pointer cast, no signed-char bugs
  if (p.company != 0xFFFF || p.magic[0] != 'N' || p.magic[1] != 'B' || p.version != 1)
    return false;

  portENTER_CRITICAL(&g_tank_mux);
  g_tank.fv_mv = p.fv_mv;
  g_tank.gv_mv = p.gv_mv;
  g_tank.seq = p.seq;
  g_tank.last_seen_ms = millis();
  portEXIT_CRITICAL(&g_tank_mux);
  return true;
}

TankState tank_snapshot() {
  portENTER_CRITICAL(&g_tank_mux);
  TankState s = g_tank;
  portEXIT_CRITICAL(&g_tank_mux);
  return s;
}

int tank_percent(uint16_t mv, uint32_t last_seen_ms,
                 uint16_t empty_mv, uint16_t full_mv,
                 uint32_t stale_ms, uint16_t min_valid_mv) {
  if (last_seen_ms == 0 || millis() - last_seen_ms > stale_ms) return -1;
  if (mv < min_valid_mv || empty_mv == full_mv) return -1;
  long pct = ((long)mv - empty_mv) * 100L / ((long)full_mv - empty_mv);
  return (int)constrain(pct, 0L, 100L);
}
