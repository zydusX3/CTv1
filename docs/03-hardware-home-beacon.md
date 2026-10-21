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
- **ESP32-S3 firmware (Arduino or ESP-IDF):**
  - WiFi manager (captive portal first time), stores MQTT server and keys.
  - Publishes `ctv1/gw/<gw_id>/up` JSON for each packet, `ctv1/gw/<gw_id>/status` every 60 s.
  - Subscribes `ctv1/gw/<gw_id>/down` for commands (buzz, interval) and queues them per tag.
  - Buffers up to ~2000 packets in flash if WiFi or the server is down, then replays.
  - Local fallback: if the server is unreachable, it can still check a cached fence and
    sound the siren / LED on its own.

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
