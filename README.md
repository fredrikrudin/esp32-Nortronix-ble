# esp32-Nortronix-ble – BLE-nivågivare för färsk- och gråvatten

En fristående ESP32 som läser av signalerna från husvagnens Nortronix-tryckbox (nivågivare för färskvatten och gråvatten) och sänder dem trådlöst som en **BLE-beacon**. Huvudskärmen (`esp32-S3-ws4-caravan` på Waveshare ESP32-S3-Touch-LCD-4) plockar upp paketen med sin befintliga NimBLE-skanner, på samma sätt som RuuviTag-sensorerna.

> **Status:** utkast, ej testat på riktig hårdvara. Signalnivåerna från Nortronix-boxen är inte verifierade – se [Mät först](#1-mät-först).

---

## Innehåll

- [Översikt](#översikt)
- [Filer](#filer)
- [Hårdvara](#hårdvara)
  - [1. Mät först](#1-mät-först)
  - [2. Komponenter](#2-komponenter)
  - [3. Kopplingsschema](#3-kopplingsschema)
  - [4. Välj spänningsdelare](#4-välj-spänningsdelare)
  - [5. Strömförsörjning](#5-strömförsörjning)
  - [6. Montering](#6-montering)
- [Sändaren (`nortronix_ble_tx`)](#sändaren-nortronix_ble_tx)
- [BLE-paketformat](#ble-paketformat)
- [Mottagaren (huvudskärmen)](#mottagaren-huvudskärmen)
- [Kalibrering](#kalibrering)
- [Felsökning](#felsökning)
- [Licens](#licens)

---

## Översikt

```text
 ┌──────────────┐  FV/GV-signal  ┌───────────────────┐   BLE-beacon    ┌──────────────────────┐
 │ Nortronix-   │───────┬───────▶│ Originalpanel     │                 │ Waveshare ESP32-S3   │
 │ tryckbox     │       │        │ (fungerar som     │                 │ (caravan-display)    │
 │ vid tankarna │       │        │  vanligt)         │                 │                      │
 └──────────────┘       │        └───────────────────┘                 │  NimBLE-skanner      │
                        │                                              │  → tank_parse()      │
                        │  spänningsdelare                             │  → kalibrering → %   │
                        ▼                                              │  → LVGL-UI           │
                 ┌──────────────┐        "NortronixBLE", mV FV/GV      │                      │
                 │ ESP32 DevKit │ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ▶│                      │
                 │ (sändare)    │        var 10:e s                    └──────────────────────┘
                 └──────────────┘
```

**Principer**

- **Passiv avläsning.** Sändaren "tjuvlyssnar" parallellt via en högohmig spänningsdelare. Originalpanelen ska inte påverkas.
- **Ingen parkoppling.** Sändaren är en ren, icke-anslutningsbar beacon. Data ligger i *Manufacturer Data* i annonspaketet.
- **Rådata i luften, kalibrering på skärmen.** Sändaren skickar signalspänningen i mV. Omräkningen till procent sker på skärmen, så sändaren behöver aldrig flashas om när kalibreringen ändras.

---

## Filer

| Fil | Var den hör hemma | Beskrivning |
|---|---|---|
| `nortronix_ble_tx/nortronix_ble_tx.ino` | Sändar-ESP32 | Läser ADC, filtrerar, sänder BLE-beacon |
| `nortronix_ble_rx/tank.h` | Huvudskärmens projekt | Deklarationer: tillstånd, parser, procentberäkning |
| `nortronix_ble_rx/tank.cpp` | Huvudskärmens projekt | Implementation |

Båda sidor använder **NimBLE-Arduino 2.x**.

---

## Hårdvara

### 1. Mät först

Innan något kopplas in – mät signalerna från tryckboxen med multimeter (helst även oscilloskop):

1. Hitta FV-, GV- och GND-ledarna mellan boxen och originalpanelen.
2. Mät spänningen FV→GND och GV→GND vid **tom** tank och vid **full** tank.
3. Notera även värdet med **motorn/laddaren igång** – 12 V-systemet kan ligga på upp till ca 14,4 V, och om signalen följer matningen följer den med upp.
4. Kontrollera om signalen finns **hela tiden** eller bara när man trycker på knappen på panelen. Många tanknivåsystem matar givaren endast vid avläsning.
5. Kontrollera att signalen är en **stadig likspänning** och inte pulsad eller multiplexad. Är den inte det fungerar inte detta upplägg utan ändringar.

Skriv ner värdena – de behövs för att välja spänningsdelare och för kalibreringen.

### 2. Komponenter

| Antal | Komponent | Kommentar |
|---|---|---|
| 1 | ESP32 DevKit V1 (klassisk ESP32) | GPIO 32 och 34 är ADC1, som fungerar samtidigt med radion |
| 2 | Motstånd R1/R3, 100 kΩ | Serie­motstånd, 1 % metallfilm |
| 2 | Motstånd R2/R4 | Väljs efter tabellen nedan, 1 % metallfilm |
| 2 | Kondensator 100 nF keramisk | Från ADC-pinne till GND, filtrerar brus |
| 1 | DC-DC buck 12 V → 5 V | Matar DevKit via 5V/VIN |
| 1 | Säkring 1 A (flatstift eller inline) | På 12 V-matningen |
| – | Kopplingsplint, krympslang, kapsling | |

> **Varför 100 kΩ och inte 10 kΩ?** Ju högre resistans, desto mindre belastas givarsignalen och desto mindre risk att originalpanelens visning ändras. Kondensatorn på 100 nF ger ADC:n en stabil källa trots den höga resistansen.

### 3. Kopplingsschema

```text
                     FÄRSKVATTEN (FV)
 Box FV-signal ──[ R1 100k ]──┬───────────────── GPIO 32 (ADC1_CH4)
                              │         │
                         [ R2 se tab ] ═╪═ 100 nF
                              │         │
 ─────────────────────────────┴─────────┴─────── GND

                     GRÅVATTEN (GV)
 Box GV-signal ──[ R3 100k ]──┬───────────────── GPIO 34 (ADC1_CH6, endast input)
                              │         │
                         [ R4 se tab ] ═╪═ 100 nF
                              │         │
 ─────────────────────────────┴─────────┴─────── GND

                     STRÖM
 Husvagn +12 V ──[ Säkring 1 A ]── Buck IN+     Buck OUT+ ── DevKit 5V/VIN
 Husvagn GND  ──────────────────── Buck IN−     Buck OUT− ── DevKit GND

 VIKTIGT: Husvagnens GND, boxens GND, buckens GND och ESP32:ans GND
          ska vara gemensam. Annars blir mätningen meningslös.
```

### 4. Välj spänningsdelare

Mål: **högsta signalspänning ska ge högst ca 2,5 V på ADC-pinnen.** Över ca 2,5 V blir ESP32:ans ADC olinjär, och över ca 3,6 V kan pinnen skadas.

Med R1 = 100 kΩ:

| Högsta uppmätta signal | R2 | Delningsfaktor | Spänning på pinnen vid max |
|---|---|---|---|
| ≤ 5 V | 68 kΩ | 2,47 | 2,02 V |
| ≤ 10 V | 33 kΩ | 4,03 | 2,48 V |
| ≤ 14,4 V (följer 12 V-matningen) | 18 kΩ | 6,56 | 2,20 V |

Formel: `V_pinne = V_signal × R2 / (R1 + R2)`

Uppdatera konstanterna i sändarkoden så att de matchar motstånden:

```cpp
constexpr float R1 = 100000.0f;
constexpr float R2 = 33000.0f;
```

### 5. Strömförsörjning

- Mata via buck-omvandlare till DevKitets **5V/VIN**-pinne. Mata aldrig 12 V direkt.
- Sätt en **säkring** närmast 12 V-uttaget.
- Strömförbrukningen beror på kortet (DevKitets USB-krets och spänningsregulator drar en del även i vila). Mät gärna med en multimeter i serie – det är lätt att räkna ut dygnsförbrukningen mot batteriet därifrån.
- Vill du spara ström senare: sänk uppdateringsfrekvensen eller använd djupsömn mellan mätningarna (se [Förbättringar framåt](#förbättringar-framåt)).

### 6. Montering

- Placera sändaren nära tryckboxen så att signalledarna blir korta. BLE räcker normalt genom en husvagn.
- Använd en kapsling som skyddar mot fukt och kondens – det är ofta fuktigt nära tankarna.
- Skarva in i signalledarna med kopplingsplint eller lödning + krympslang, inte med "tjuvkopplingar" som kan glappa.
- Undvik att placera ESP32:an direkt mot metall eller bakom tankar fyllda med vatten, som dämpar radiosignalen.

---

## Sändaren (`nortronix_ble_tx`)

**Bygga och ladda upp**

1. Arduino IDE eller PlatformIO med **arduino-esp32**-kärnan.
2. Installera biblioteket **NimBLE-Arduino** (version 2.x).
3. Välj kortet *ESP32 Dev Module*.
4. Justera `R1`/`R2` efter dina motstånd.
5. Ladda upp och öppna seriemonitorn på 115200 baud. Du ska se rader som:

```text
seq 12  FV 3412 mV  GV 1187 mV
```

**Vad koden gör**

| Steg | Beskrivning |
|---|---|
| Mätning | 16 avläsningar per kanal med `analogReadMilliVolts()` (använder chipets fabrikskalibrering). De 4 lägsta och 4 högsta kastas, resten medelvärdesbildas. |
| Omräkning | Pinnens mV × delningsfaktorn = signalens mV före spänningsdelaren. |
| Sändning | Nytt paket var 10:e sekund (`UPDATE_MS`). Annonsintervall ca 1 s. Icke-anslutningsbar beacon. |

**Inställningar**

| Konstant | Standard | Betydelse |
|---|---|---|
| `PIN_FV` / `PIN_GV` | 32 / 34 | ADC-pinnar. Använd endast ADC1-pinnar (32–39) – ADC2 fungerar inte samtidigt med radion. |
| `R1` / `R2` | 100k / 33k | Spänningsdelarens motstånd |
| `UPDATE_MS` | 10000 | Hur ofta nya mätvärden skickas |
| `ADV_INT_MIN` / `ADV_INT_MAX` | 1600 / 1760 | Annonsintervall i enheter om 0,625 ms (1,0–1,1 s) |

---

## BLE-paketformat

Annonspaketet innehåller flaggor (`0x06`), namnet `NortronixBLE` och följande Manufacturer Data (10 byte, little endian):

| Offset | Storlek | Fält | Värde / beskrivning |
|---|---|---|---|
| 0 | 2 | `company` | `0xFFFF` (Bluetooth SIG:s test-ID, skickas som `FF FF`) |
| 2 | 2 | `magic` | `'N' 'B'` – skiljer paketet från andra enheter som också använder `0xFFFF` |
| 4 | 1 | `version` | `1` – öka vid ändrat format |
| 5 | 1 | `seq` | Löpnummer, ökar för varje ny mätning (slår om efter 255) |
| 6 | 2 | `fv_mv` | Färskvattensignal i mV (före spänningsdelaren) |
| 8 | 2 | `gv_mv` | Gråvattensignal i mV (före spänningsdelaren) |

Totalt annonspaket: 3 (flaggor) + 14 (namn) + 12 (manufacturer data) = **29 av 31 byte**.

Exempel på rå Manufacturer Data: `FF FF 4E 42 01 0C 54 0D A3 04` → seq 12, FV 3412 mV, GV 1187 mV.

---

## Mottagaren (huvudskärmen)

1. Kopiera `tank.h` och `tank.cpp` till skärmprojektet.
2. Anropa parsern först i din befintliga scan-callback:

```cpp
#include "tank.h"

void onResult(const NimBLEAdvertisedDevice* dev) override {
    if (tank_parse(dev)) return;      // var vårt tankpaket – klart
    // ... befintlig kod för RuuviTag och Victron ...
}
```

> Signaturen för `onResult` skiljer sig mellan NimBLE-Arduino 1.x och 2.x. Anpassa efter den version projektet använder.

3. Läs värdena i UI-tasken via en trådsäker kopia:

```cpp
TankState t = tank_snapshot();
int fv = tank_percent(t.fv_mv, t.last_seen_ms, FV_EMPTY_MV, FV_FULL_MV);
int gv = tank_percent(t.gv_mv, t.last_seen_ms, GV_EMPTY_MV, GV_FULL_MV);

if (fv < 0) lv_label_set_text(label_fv, "--");
else        lv_label_set_text_fmt(label_fv, "%d %%", fv);
```

**`tank_percent()` returnerar −1** (visa "--") när:

- inget paket tagits emot ännu,
- senaste paketet är äldre än 60 s (`stale_ms`),
- signalen är under 100 mV (`min_valid_mv`) – tolkas som att boxen saknar ström, inte som tom tank.

**Dubblettfilter:** Om skannern kör `setDuplicateFilter(true)` kan upprepade annonser från samma MAC-adress filtreras bort i radiokontrollern. Kontrollera att nya tankvärden faktiskt kommer fram (titta på att `seq` ökar).

**Versionshantering:** När filerna läggs in i `esp32-S3-ws4-caravan` – lägg till versionstagg i filhuvudet och en rad i changeloggen.

---

## Kalibrering

1. Läs av `fv_mv` och `gv_mv` i seriemonitorn (sändaren) eller i skärmens logg.
2. Notera värdet vid **tom** tank och vid **full** tank för respektive tank.
3. Lägg in värdena på skärmen som `FV_EMPTY_MV`, `FV_FULL_MV`, `GV_EMPTY_MV`, `GV_FULL_MV` – helst som inställningar som kan ändras via webbsidan, så att ingen omkompilering behövs.
4. Givaren får gärna vara inverterad (högre spänning vid tom tank) – `tank_percent()` hanterar det.

> Tryckbaserade givare mäter vattenpelarens höjd. Om tanken inte har raka väggar blir sambandet mellan spänning och volym inte linjärt. Vill du ha exakt volym kan kalibreringen byggas ut med flera punkter (t.ex. tom, ¼, ½, ¾, full).

---

## Felsökning

| Symptom | Trolig orsak | Åtgärd |
|---|---|---|
| Visar alltid "--" | Inga paket tas emot | Kontrollera med en BLE-skanner i mobilen (t.ex. nRF Connect) att `NortronixBLE` syns. Kontrollera dubblettfiltret. |
| Visar "--" men paket syns | Signal < 100 mV | Boxen matas troligen bara vid knapptryck. Se [Mät först](#1-mät-först). |
| Värdet hoppar | Brus eller dålig jord | Kontrollera gemensam GND, kondensatorerna på 100 nF och korta signalledare. |
| Värdet fastnar på max | Pinnen mättas över ca 2,5 V | Välj ett mindre R2 enligt tabellen. |
| Originalpanelen visar annorlunda | Spänningsdelaren belastar signalen | Öka R1 och R2 proportionellt (t.ex. 220k/68k). |
| Kompileringsfel i BLE-koden | Fel NimBLE-version | Koden är skriven för NimBLE-Arduino 2.x. |

---

## Förbättringar framåt

- **Djupsömn** mellan mätningarna för lägre strömförbrukning (kräver att skärmen skannar kontinuerligt för att inte missa korta sändningsfönster).
- **Batterispänning/matningsspänning** som extra fält i paketet (öka `version`).
- **Kalibrering via webbsidan** på caravan.local.
- **Larm** på skärmen vid låg färskvattennivå eller hög gråvattennivå.

---

## Licens

Creative Commons Erkännande-IckeKommersiell (CC BY-NC), samma som `esp32-S3-ws4-caravan`. Koden är skriven med hjälp av Claude AI.
