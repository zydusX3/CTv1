# CTv1 task list

The single list of what's done, what's next and what's been dropped. Updated at the end of every
piece of work and whenever a requirement is added or dropped.

**How to use it**
- Every task has an ID (`T-012`) that never changes, so commits and chats can refer to it.
- Status: `[ ]` to do · `[~]` in progress · `[x]` done · `[-]` dropped (moved to *Dropped* with the reason).
- New ideas go in *Inbox* first, then get an ID and a place in *Now*, *Next* or *Later*.
- Every change to this list gets a line in the *Change log* at the bottom.

---

## Decisions needed (from you)

| ID | Question | Why it matters | Current assumption |
|---|---|---|---|
| D-1 | Which country / LoRa band? | Module variant to buy, legal frequency and power | India, IN865 (865.0625 MHz, 14 dBm) |
| D-2 | How many animals in v1, and the paddock size? | Number of collars to build, channel load, antenna | 8–20 animals, 1–3 km from the house |
| D-3 | Where will the server run: Raspberry Pi at home, or a small cloud VPS? | Server setup and how phones reach it from outside | Raspberry Pi at home + Tailscale |
| D-4 | Alerts by Telegram, SMS, app push, or a mix? | What to build first in the alert sender | Telegram first (free, easy) |
| D-5 | Budget per collar? | Solar or not, GPS module choice (MAX-M10S vs ATGM336H) | about 40–60 USD |

---

## Now

| ID | Task | Notes |
|---|---|---|
| T-020 | **Order bench-test parts** | Long shipping times, so start now. See *Bench kit* below. Needs D-1 for the module band. |
| T-021 | **Tag simulator** | Python script that publishes realistic `ctv1/gw/home/up` messages to MQTT, the same way the mockup's herd moves. Lets the server and app be built before any hardware arrives. |
| T-022 | **Server MVP** | Mosquitto + small backend (Python FastAPI) + SQLite to start: store reports, keep the latest position per tag, server-side fence check with hysteresis, REST + WebSocket for the app. Runs in Docker on a Pi or PC. Spec: docs/06. |

## Next

| ID | Task | Notes |
|---|---|---|
| T-023 | Alerts: Telegram bot | Left fence, entered keep-out, no update 30 min, low battery, beacon offline, power cut. Repeat critical alerts until acknowledged. |
| T-024 | Real app front end | Turn the mockup into the real PWA: Leaflet with OpenStreetMap + satellite tiles, live data over WebSocket, Simple / Advanced views, fence editor that publishes the fence to the beacon. |
| T-025 | Bench bring-up: radio link | Tag + beacon radio on the desk with `gw_monitor.py`: reports, ACKs, retries, `DUP`, `ATC+` settings, buzz command. |
| T-026 | Bench bring-up: tag power | Motion wake on PA0, `still` GPS skip, battery ADC calibration, UBX "GGA+RMC only", sleep current target < 50 µA (PPK2 or µCurrent). |
| T-027 | Bench bring-up: home beacon | ESP32 + radio together, setup hotspot on iPhone/Android, MQTT to the server, offline buffer replay, mains-loss event, siren output. |
| T-028 | Bench bring-up: handheld | OLED (SH1106/SSD1306), LSM303AGR axes + calibration, arrow accuracy, buzz delivered at +400 ms after the beacon's ACK. |
| T-029 | Range test | Roof antenna vs paddock: RSSI/SNR map with a tag carried around, SF9 vs SF10. |
| T-030 | CI on GitHub | Run `make -C firmware/test` on every push; later also compile all sketches. |

## Later

| ID | Task | Notes |
|---|---|---|
| T-040 | Collar PCB in KiCad | From docs/02 schematic: RAK3172, MAX-M10S, LIS3DH, LDO, load switch, charger, buzzer. |
| T-041 | Collar enclosure + strap | IP67 box, GPS on top / battery as counterweight, under 350 g. |
| T-042 | Home beacon PCB / enclosure | Or a Heltec V3 + ESP32 dev board in a box for v1. |
| T-043 | Handheld PCB + case | |
| T-044 | Field test with 1 animal, then 3 | Battery per day, fix times, false fence alerts, collar comfort. |
| T-045 | Encrypt reports + 32-bit counter | v1 only signs packets: positions are readable by anyone listening, and replays are weakly protected. |
| T-046 | History playback + reports | Route for any day, distance per day, resting vs grazing time. |
| T-047 | Beacon OLED status screen | Pins are reserved (GPIO 8/9). The web page covers it for now. |
| T-048 | LTE / SMS fallback on the beacon | For when WiFi or mains power is down (docs/05). |
| T-049 | Behaviour analytics | Grazing / resting / walking from the accelerometer; heat and sickness hints (docs/05). |
| T-050 | Tech stack doc | `docs/07-tech-stack.md`: RUI3, LoRa P2P, ESP32, MQTT, tooling (from the chat explanation). |

## Inbox

_New ideas and requests go here until they're sorted._

---

## Bench kit (for T-020)

| Qty | Part | For |
|---|---|---|
| 3 | RAK3172 breakout / evaluation board (band from D-1) | tag, beacon radio, handheld |
| 2 | u-blox MAX-M10S breakout with patch antenna | tag, handheld |
| 1 | LIS3DH breakout | tag |
| 1 | LSM303AGR breakout | handheld |
| 1 | 1.3" SH1106 I²C OLED | handheld |
| 1 | ESP32-S3-DevKitC-1 **N8R2** | beacon |
| 2 | 3.3 V USB-serial adapter (CP2102 / CH340) | flashing + logs |
| 3 | 868/915 MHz antennas (u.FL / SMA to match the boards) | all |
| 2 | 18650 cell + holder, TP4056 or CN3791 module | power tests |
| — | breadboards, jumper wires, buttons, small buzzer, 2N7002 | |
| 1 | Nordic PPK2 (optional, about 100 USD) | measuring sleep current |

---

## Done

| ID | Task | Where |
|---|---|---|
| T-001 | System design: block diagrams, radio plan, packet format, power budget | docs/01 |
| T-002 | Collar tag schematic + BOM | docs/02 |
| T-003 | Home beacon schematic + BOM | docs/03 |
| T-004 | Handheld schematic + BOM | docs/04 |
| T-005 | Expansion options | docs/05 |
| T-006 | App requirements, MQTT topics, data model | docs/06 |
| T-007 | Interactive UI mockup (Advanced view) | app/mockup |
| T-008 | Simple view for elderly users: map only, big zoom, Advanced switch | app/mockup |
| T-009 | Simple view status line ("All 8 cows are safe") | app/mockup |
| T-010 | Full daily route in Simple view, double-tap zoom, touch pan/pinch | app/mockup |
| T-011 | Shared firmware library + host unit tests (228 checks) | firmware/lib, firmware/test |
| T-012 | Collar tag firmware | firmware/tag |
| T-013 | Home beacon radio firmware + PC bench monitor | firmware/gateway_radio, firmware/tools |
| T-014 | Home beacon ESP32-S3 firmware: WiFi setup, MQTT, offline buffer, backup fence alarm, web page | firmware/beacon_esp32 |
| T-015 | Settings kept in flash, `ATC+` commands (one build for all collars) | lib/ctv1 settings + rui3 |
| T-016 | GPS power saving: skip when still, back-off, UBX GGA+RMC only | lib/ctv1 power, tag |
| T-017 | Handheld firmware: arrow, compass + calibration, buzz / fast-track | firmware/handheld |
| T-018 | RUI3 "LoRa P2P only" build: about half the flash on every RAK3172 | firmware/README |

## Dropped

| ID | Task | Why |
|---|---|---|
| — | _nothing yet_ | |

---

## Change log

| Date | Change |
|---|---|
| 2026-10-22 | List created. T-001…T-018 recorded as done. Now: T-020 parts, T-021 simulator, T-022 server MVP. Decisions D-1…D-5 opened. |
