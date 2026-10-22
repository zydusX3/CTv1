# CTv1 task list

The single list of what's done, what's next and what's been dropped. Updated at the end of every
piece of work and whenever a requirement is added or dropped.

**How to use it**
- Every task has an ID (`T-012`) that never changes, so commits and chats can refer to it.
- Status: `[ ]` to do · `[~]` in progress · `[x]` done · `[-]` dropped (moved to *Dropped* with the reason).
- New ideas go in *Inbox* first, then get an ID and a place in *Now*, *Next* or *Later*.
- Every change to this list gets a line in the *Change log* at the bottom.

---

## Decisions

| ID | Question | Decision (2026-10-22) | What it means |
|---|---|---|---|
| D-1 | Country / LoRa band | **India (IN865)**, configurable for other countries later | Defaults 865.0625 MHz, 20 dBm. Per device with `ATC+CTFREQ` / `ATC+CTPWR`; region presets in T-055 |
| D-2 | Herd size and range | **5 animals, 5 km** default, configurable | SF10 default (`ATC+CTSF` to change). Firmware handles up to 32–64 tags |
| D-3 | Server | **No server in v1.** Raspberry Pi server becomes an add-on | The home beacon is the hub: storage, alerts, app (docs/06 "v1: no server") |
| D-4 | Alerts | **App push** | ntfy push first (no app to build), Firebase push with our own app later (T-054, T-057) |
| D-5 | Budget | **Minimal now, modular** so modules can be upgraded per client | Carrier boards with standard plug-in ports (hardware/README.md) |

---

## Now

| ID | Task | Notes |
|---|---|---|
| T-020 | **Order bench-test parts** | IN865 modules. See *Bench kit* below and the wiring sheets in hardware/. |
| T-021 | **Simulated herd in the beacon firmware** | Build option that feeds realistic reports into the ESP32 as if from the radio, so the app can be built with just an ESP32 DevKitC. |
| T-051 | **App served by the home beacon** | Simple + Advanced views (from the mockup) stored on the ESP32, opened at `http://ctv1-beacon.local` on home WiFi. REST API (`/api/herd`, `/api/history`, `/api/fence`, `/api/cmd`) and WebSocket live updates. Fence editor saves to the beacon. |
| T-052 | **History on the beacon** | Every report stored in flash (LittleFS ring buffer, ~11 KB/day for 5 animals), daily routes for the "full route" view. |

## Next

| ID | Task | Notes |
|---|---|---|
| T-053 | Alert rules on the beacon | Left fence / keep-out (exists), no update 30 min, low battery, no movement 4 h, night movement, mains lost, radio silent. |
| T-054 | Push notifications, step 1: ntfy | Beacon posts each alert to a private ntfy topic; the free ntfy app shows it on the phone. Needs internet at the house. |
| T-055 | Region presets | `ATC+CTREGION=IN865` etc. sets frequency + legal power at once; EU868 (14 dBm, 1 % duty) and US915 / AU915 notes. |
| T-056 | Configurable herd and range | Max tags and default SF in settings; range guide (SF9 ~3 km, SF10 ~5 km, SF11 hills). |
| T-025 | Bench bring-up: radio link | Tag + beacon radio with `gw_monitor.py`: reports, ACKs, retries, `DUP`, `ATC+` settings, buzz. |
| T-026 | Bench bring-up: tag power | Motion wake on PA0, `still` GPS skip, battery ADC calibration, UBX GGA+RMC only, sleep current < 50 µA. |
| T-027 | Bench bring-up: home beacon | ESP32 + radio, setup hotspot on iPhone/Android, app, history, push, mains-loss event, siren. |
| T-028 | Bench bring-up: handheld | OLED, LSM303AGR axes + calibration, arrow accuracy, buzz after the beacon's ACK. |
| T-029 | Range test | Roof antenna vs paddock: RSSI/SNR map, SF9 vs SF10 vs SF11. |
| T-030 | CI on GitHub | `make -C firmware/test` and `python3 hardware/gen/render.py` on every push; later compile all sketches. |

## Later

| ID | Task | Notes |
|---|---|---|
| T-057 | Own phone app with Firebase push (step 2) | Same UI wrapped as an Android/iOS app (Capacitor or Flutter); beacon sends pushes through Firebase. |
| T-022 | Server add-on (Raspberry Pi) | Remote map access from anywhere, long history, several beacons/users. MQTT side already in the beacon. |
| T-040 | Collar PCB in KiCad | Start from hardware/tag (netlist, placement). |
| T-041 | Collar enclosure + strap | IP67 box, GPS on top / battery as counterweight, under 350 g. |
| T-042 | Home beacon PCB + box | Start from hardware/beacon. |
| T-043 | Handheld PCB + case | Start from hardware/handheld. |
| T-044 | Field test with 1 animal, then 5 | Battery per day, fix times, false fence alerts, collar comfort. |
| T-045 | Encrypt reports + 32-bit counter | v1 only signs packets (readable by listeners, weak replay protection). |
| T-046 | History playback + reports | Any day's route, distance per day, resting vs grazing time. |
| T-047 | Beacon OLED status screen | Port DSP-4 is on the beacon board already. |
| T-048 | LTE / SMS fallback on the beacon | For no internet / mains down (docs/05). |
| T-049 | Behaviour analytics | Grazing / resting / walking; heat and sickness hints (docs/05). |
| T-050 | Tech stack doc | `docs/07-tech-stack.md`: RUI3, LoRa P2P, ESP32, MQTT, tooling. |

## Inbox

_New ideas and requests go here until they're sorted._

---

## Bench kit (for T-020)

| Qty | Part | For |
|---|---|---|
| 3 | RAK3172 breakout / evaluation board, **IN865** variant | tag, beacon radio, handheld |
| 2 | u-blox MAX-M10S breakout with patch antenna | tag, handheld |
| 1 | LIS3DH breakout | tag |
| 1 | LSM303AGR breakout | handheld |
| 1 | 1.3" SH1106 I²C OLED | handheld |
| 1 | ESP32-S3-DevKitC-1 **N8R2** | beacon |
| 2 | 3.3 V USB-serial adapter (CP2102 / CH340) | flashing + logs |
| 3 | 865 MHz antennas (u.FL / SMA to match the boards) | all |
| 2 | 18650 or LiPo + TP4056 USB-C module + 3.3 V LDO board | power |
| — | breadboards, jumper wires, 3 buttons, buzzer, coin motor, 2N7002, 1N4148, LEDs, 1M / 100k / 330 Ω resistors | |
| 1 | Nordic PPK2 (optional, about 100 USD) | sleep current |

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
| T-011 | Shared firmware library + host unit tests | firmware/lib, firmware/test |
| T-012 | Collar tag firmware | firmware/tag |
| T-013 | Home beacon radio firmware + PC bench monitor | firmware/gateway_radio, firmware/tools |
| T-014 | Home beacon ESP32-S3 firmware: WiFi setup, MQTT, offline buffer, backup fence alarm, web page | firmware/beacon_esp32 |
| T-015 | Settings kept in flash, `ATC+` commands (one build for all collars) | lib/ctv1 settings + rui3 |
| T-016 | GPS power saving: skip when still, back-off, UBX GGA+RMC only | lib/ctv1 power, tag |
| T-017 | Handheld firmware: arrow, compass + calibration, buzz / fast-track | firmware/handheld |
| T-018 | RUI3 "LoRa P2P only" build: about half the flash on every RAK3172 | firmware/README |
| T-019 | India defaults (SF10, 20 dBm) and reply timing worked out per SF (no beacon/handheld overlap at SF7-12) | lib/ctv1 protocol, configs (244 test checks) |
| T-058 | Hardware reference set: plug-in port standard, bench wiring, reference schematics, PCB placements, netlists, BOMs, generated and checked against the firmware pins | hardware/ |

## Dropped

| ID | Task | Why |
|---|---|---|
| T-023 | Telegram alert bot | D-4: app push instead. Can come back with the server add-on. |
| T-024 | Separate app front end for the server | Merged into T-051: the same app is served by the beacon. |

---

## Change log

| Date | Change |
|---|---|
| 2026-10-22 | List created. T-001…T-018 recorded as done. Now: T-020 parts, T-021 simulator, T-022 server MVP. Decisions D-1…D-5 opened. |
| 2026-10-22 | Decisions D-1…D-5 made. No server in v1: T-022 moved to Later as an add-on; T-021 re-scoped to a simulated herd in the beacon; new T-051…T-057 (app on the beacon, history, alerts, ntfy push, region presets, configurable herd, own app). T-023 and T-024 dropped. T-019 (India defaults, reply timing) and T-058 (hardware reference set) done. |
