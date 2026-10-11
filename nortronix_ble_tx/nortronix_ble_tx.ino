// nortronix_ble_tx.ino — esp32-Nortronix-ble: Nortronix tank tap -> BLE broadcast
// Board: classic ESP32 (DevKit V1). Library: NimBLE-Arduino 2.x
//
// Sends raw signal voltage (mV) for fresh (FV) and grey (GV) water.
// Calibration to % is done on the display, so the sensor never needs
// reflashing when the calibration changes.

#include <NimBLEDevice.h>
#include <algorithm>

// ---------- Hardware ----------
constexpr uint8_t PIN_FV = 32;              // ADC1_CH4 (ADC1 = OK together with radio)
constexpr uint8_t PIN_GV = 34;              // ADC1_CH6, input only

// Voltage divider: signal -- R1 -- pin -- R2 -- GND
// Choose so that MAX measured signal gives <= ~2.5 V at the pin.
// Put 100 nF from each ADC pin to GND.
constexpr float R1 = 100000.0f;
constexpr float R2 = 33000.0f;
constexpr float DIVIDER = (R1 + R2) / R2;

// ---------- Timing ----------
constexpr uint32_t UPDATE_MS    = 10000;    // tanks change slowly
constexpr uint16_t ADV_INT_MIN  = 1600;     // x 0.625 ms = 1.0 s
constexpr uint16_t ADV_INT_MAX  = 1760;     // 1.1 s

// ---------- BLE payload ----------
constexpr uint16_t COMPANY_ID = 0xFFFF;     // SIG "test/no company" ID
constexpr uint8_t  MAGIC0 = 'N', MAGIC1 = 'B', PAYLOAD_VER = 1;

struct __attribute__((packed)) TankAdv {
  uint16_t company;     // little endian (ESP32 is LE)
  uint8_t  magic[2];    // 'N','B' — separates us from other 0xFFFF devices
  uint8_t  version;
  uint8_t  seq;         // increments per update, lets receiver see fresh data
  uint16_t fv_mv;       // signal voltage before divider, mV
  uint16_t gv_mv;
};
static_assert(sizeof(TankAdv) == 10, "payload size");

static NimBLEAdvertising* adv = nullptr;
static uint8_t seq = 0;

// Median-trimmed average of N calibrated readings (eFuse-calibrated mV)
static uint16_t readSignalMv(uint8_t pin) {
  constexpr int N = 16;
  uint32_t s[N];
  for (int i = 0; i < N; i++) {
    s[i] = analogReadMilliVolts(pin);
    delayMicroseconds(250);
  }
  std::sort(s, s + N);
  uint32_t sum = 0;
  for (int i = 4; i < 12; i++) sum += s[i];          // drop 4 lowest/highest
  float sig = (sum / 8.0f) * DIVIDER;
  return (uint16_t)std::min(sig, 65535.0f);
}

static void publish(uint16_t fv, uint16_t gv) {
  TankAdv p{COMPANY_ID, {MAGIC0, MAGIC1}, PAYLOAD_VER, seq++, fv, gv};

  NimBLEAdvertisementData d;
  d.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);   // 0x06
  d.setName("NortronixBLE");
  d.setManufacturerData(reinterpret_cast<const uint8_t*>(&p), sizeof(p));
  // 3 (flags) + 14 (name) + 12 (manuf) = 29 of 31 bytes

  adv->stop();
  adv->setAdvertisementData(d);
  adv->start();
}

void setup() {
  Serial.begin(115200);
  // Default attenuation is 11 dB (~0.15–2.45 V accurate range); nothing to set.

  NimBLEDevice::init("NortronixBLE");
  adv = NimBLEDevice::getAdvertising();
  adv->setConnectableMode(BLE_GAP_CONN_MODE_NON);   // pure beacon
  adv->setMinInterval(ADV_INT_MIN);
  adv->setMaxInterval(ADV_INT_MAX);
}

void loop() {
  static uint32_t last = 0;
  if (last != 0 && millis() - last < UPDATE_MS) { delay(50); return; }
  last = millis();

  uint16_t fv = readSignalMv(PIN_FV);
  uint16_t gv = readSignalMv(PIN_GV);
  publish(fv, gv);

  Serial.printf("seq %u  FV %u mV  GV %u mV\n", (unsigned)(seq - 1), fv, gv);
}
