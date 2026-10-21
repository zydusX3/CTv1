# 03 · Home beacon (gateway) hardware

The home beacon stays powered all the time, so battery life is not a concern here.
It uses the same RAK3172 radio as the tags (same firmware library, same packet format)
and an ESP32-S3 for WiFi, MQTT and a small setup web page.

> **Fast bring-up option:** a Heltec WiFi LoRa 32 V3 or LilyGO T3-S3 board
> (ESP32-S3 + SX1262 on one board) works as a gateway with zero soldering. Pins on
> Heltec V3: NSS 8, SCK 9, MOSI 10, MISO 11, RST 12, BUSY 13, DIO1 14. Use it to test
> the tags first, then build the version below for the roof.

## Schematic

```
                         POWER (with UPS so a power cut does not lose alerts)
 5 V / 2 A USB-C adapter
   VBUS ──┬───────────────────────────────►  VBUS_SENSE: 100 kΩ / 100 kΩ divider → ESP GPIO 1
          │
          │      ┌───────────────────────┐
          └─────►│ 5V IN  IP5306 module  │ BAT ──── 18650 3.4 Ah (backup ≈ 12 h)
                 │ (charge + 5 V boost)  │
                 │ 5V OUT                ├──┬──► 5V_SYS ── (optional siren driver)
                 └───────────────────────┘  │
                                       ┌────┴─────┐
                                       │ AP2112K  │ 3V3 (600 mA)
                                       │ 3.3 V    ├──┬──────► 3V3 rail
                                       └────┬─────┘ 10µF + 100µF
                                           GND

                         RADIO  ⇄  WIFI BRIDGE
   ┌──────────────────────────┐                ┌────────────────────────────────┐
   │ RAK3172                  │                │ ESP32-S3-WROOM-1 (N8R2)        │
   │ VDD ◄── 3V3              │                │ 3V3 ◄── 3V3      EN ── 10k↑ + 1µF
   │ PB6 UART1_TX ────────────┼───────────────►│ GPIO 18 (UART1 RX)             │
   │ PB7 UART1_RX ◄───────────┼────────────────│ GPIO 17 (UART1 TX)             │
   │ PA2/PA3 UART2 → 4-pin header (programming + AT console)                   │
   │ NRST ◄───────────────────┼────────────────│ GPIO 16 (reset the radio)      │
   │ PA0  (packet-ready IRQ) ─┼───────────────►│ GPIO 15                        │
   │ RF ── u.FL pigtail       │                │                                │
   └─────────┬────────────────┘                │ GPIO 8  I2C SDA ─┐ 0.96" OLED   │
             │                                 │ GPIO 9  I2C SCL ─┘ SSD1306     │
             ▼                                 │ GPIO 4  ── 330 Ω ──►|  green "RX"│
   u.FL → N-type bulkhead                      │ GPIO 5  ── 330 Ω ──►|  red "ALARM"
   → LMR-240 coax (keep < 5 m)                 │ GPIO 6  ── SIREN_EN (below)    │
   → gas-discharge lightning arrester          │ GPIO 0  ── BOOT button          │
     (bonded to earth rod)                     │ GPIO 19/20 USB D−/D+ (program)  │
   → 5.8 dBi 868/915 MHz fibreglass antenna    │ GPIO 1  ◄─ VBUS_SENSE          │
     mounted as high as possible (roof/mast)   └────────────────────────────────┘

                         OPTIONAL SIREN / RELAY OUTPUT
 12 V siren (+) ── 12 V supply                     (or a 5 V relay module on 5V_SYS)
 12 V siren (−) ── D  IRLZ44N / AO3400
 SIREN_EN ── 220 Ω ── G       S ── GND
              100 kΩ to GND     flyback diode across siren
```

## Software on the home beacon

- **RAK3172 firmware** (`firmware/gateway_radio`): permanent RX on the farm channel. On each valid packet: check MIC,
  send ACK (+ any queued command for that tag) inside the tag's 1 s RX window, then pass
  `{raw packet, RSSI, SNR}` to the ESP32 over UART1 as one text line (`UP,-97,7,<hex>`).
  UART2 stays free for flashing, so the ESP32 never has to be unplugged to update the radio.
- **ESP32-S3 firmware** (`firmware/beacon_esp32`, Arduino):
  - First start: opens the hotspot `CTv1-Beacon-XXXX` (password `ctv1setup`). Join it and the
    settings page opens by itself: pick the WiFi, enter the MQTT server, done. Hold **BOOT**
    while powering up to get the hotspot again. It also reappears if WiFi is lost for over a minute.
  - Publishes every report to `ctv1/gw/<name>/up` as decoded JSON, status every 60 s, and events
    (fence, power cut, commands). Topics are in [06 · App requirements](06-app-requirements.md#mqtt-topics).
  - Commands from `ctv1/gw/<name>/down` go to the radio, which sends them with the tag's next ACK.
  - Keeps up to 2000 reports in RAM while WiFi or the server is down, then replays them in order with
    their original time. The UPS keeps this through power cuts.
  - Backup fence: the server publishes the paddock outline (retained) to `ctv1/gw/<name>/fence`. The
    beacon then sounds the siren and lights the red LED on its own when a tag leaves, even with no internet.
  - Web page at `http://ctv1-beacon.local`: every tag's last position (OpenStreetMap link),
    battery and signal, plus a **Buzz** button. Settings can be password-protected.
  - Resets the RAK3172 if it goes silent for 3 minutes. Reports mains power lost / restored.
  - LEDs: green blinks per report; red is on for an alarm and blinks slowly when WiFi, the server or the radio is down.
    A short press on **BOOT** silences the alarm.
  - The OLED (GPIO 8/9) is not driven yet. The web page shows the same information.

## Bill of materials

| Qty | Part | Approx. cost (USD) |
|---|---|---|
| 1 | RAK3172 (with IPEX) | 8 |
| 1 | ESP32-S3-WROOM-1 dev board | 6–8 |
| 1 | IP5306 UPS/boost module + 18650 holder + cell | 6 |
| 1 | AP2112K-3.3 LDO + caps | 1 |
| 1 | 0.96" SSD1306 OLED | 2 |
| 1 | 5.8 dBi fibreglass antenna (N-type) | 15–25 |
| 1 | LMR-240 cable 3–5 m, N connectors, u.FL→N pigtail | 10 |
| 1 | Lightning arrester (N-type, gas discharge) | 8 |
| 1 | IP65 box for the electronics (inside, near the antenna) | 6 |
| | **Total** | **≈ 60–75** |

Next: [04 · Handheld beacon hardware](04-hardware-handheld.md)
