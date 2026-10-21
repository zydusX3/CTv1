# CTv1 firmware

| Folder | Runs on | Status |
|---|---|---|
| `lib/ctv1/` | everything (plain C++) | Packet format, AES-CMAC, NMEA parser, geofence, battery, scheduling, beacon line protocol, MQTT JSON. **153 unit-test checks pass on a PC.** |
| `tag/` | collar tag, RAK3172 | Builds (88 % flash). Not yet run on hardware. |
| `gateway_radio/` | home beacon, its RAK3172 | Builds (87 % flash). Not yet run on hardware. |
| `tools/gw_monitor.py` | your PC | Shows decoded tag reports from the home beacon over USB and sends commands. |
| `beacon_esp32/` | home beacon, its ESP32-S3 | WiFi setup hotspot, MQTT bridge, offline buffer, backup fence alarm + siren, local web page. Builds (1.13 MB, 33 % of a 3 MB app partition). Not yet run on hardware. |
| Handheld beacon | | Later |

The RAK3172 sketches are built with the RAKwireless **RUI3** Arduino core 4.3.0; `beacon_esp32` with
the Espressif **Arduino-ESP32** core 3.3.12.
RUI3 itself takes about 171 KB of the 200 KB flash, so about 29 KB is left for our code.
The tag uses about 6.5 KB of that today.

## How a report works

```
tag                                          home beacon radio                  ESP32 / PC
 │ RTC timer fires (10 min ± 30 s jitter)
 │ GPS on → NMEA until HDOP ≤ 2.5 (≤ 45 s) → GPS off
 │ read battery, motion count, fence check
 │── 20 B uplink (MIC) ───────────────────────►│ check MIC with key derived from the farm key
 │                                             │── "UP,-97,7,<hex>" ──────────────►│
 │◄──────────── 10 B ACK (+ command) ── 50 ms ─│
 │ no ACK? one retry after 5–15 s, same seq    │ same seq again → re-ACK, "DUP" line only
 │ run command (buzz / interval / fast)        │
 │ sleep (Stop2) until the next timer or motion
```

## Set up the tools (Arduino IDE 2)

1. **File → Preferences → Additional boards manager URLs**, add
   `https://raw.githubusercontent.com/RAKWireless/RAKwireless-Arduino-BSP-Index/main/package_rakwireless.com_rui_index.json`
2. **Boards Manager** → install **RAKwireless RUI STM32 Boards** (4.3.0 or newer).
3. **Board:** *WisDuo RAK3172 Evaluation Board*. This board definition also works for a bare RAK3172 on our own PCB.
4. Install the shared library. Either copy or link `firmware/lib/ctv1` into your
   `Arduino/libraries` folder, or zip that folder and use **Sketch → Include Library → Add .ZIP Library**.

With arduino-cli:

```sh
arduino-cli compile -b rak_rui:stm32:WisDuoRAK3172EvaluationBoard --library firmware/lib/ctv1 firmware/tag
arduino-cli compile -b rak_rui:stm32:WisDuoRAK3172EvaluationBoard --library firmware/lib/ctv1 firmware/gateway_radio
```

### ESP32-S3 (home beacon)

1. **Boards Manager** → install **esp32 by Espressif Systems** (3.x).
2. **Library Manager** → install **PubSubClient** by Nick O'Leary. Also install `firmware/lib/ctv1` as above.
3. **Board:** *ESP32S3 Dev Module*. For an ESP32-S3-WROOM-1 **N8R2**: Flash Size **8MB**,
   Partition Scheme **8M with spiffs (3MB APP/1.5MB SPIFFS)**, PSRAM **QSPI PSRAM**, USB CDC On Boot **Enabled**.
   The default 4 MB / 1.3 MB app scheme also fits for now (85 %), but leaves little room.

```sh
arduino-cli compile -b espressif:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=enabled \
  --library firmware/lib/ctv1 firmware/beacon_esp32
```

## Before flashing

- **Farm key:** make your own and put the same value in `tag/config.h` and `gateway_radio/config.h`:
  `python3 -c "import os; print(', '.join('0x%02x' % b for b in os.urandom(16)))"`
- **Frequency:** set `LORA_FREQ_HZ` to a frequency that is legal where you are, and use the
  same value in both files. The example is IN865.
- **Tag ID:** give every collar a different `TAG_ID` (1–65535).

## Flashing

The RAK3172 is programmed through **UART2 (PA2/PA3)** with a 3.3 V USB-serial adapter
(TX→PA3, RX→PA2, GND, 3V3). Upload from the IDE as normal; RUI3 resets the module into its
bootloader with an AT command.

When `gateway_radio` runs with `LINK_ON_USB 1`, the AT commands on that port are switched
off. To upload again, pull **BOOT0 to 3.3 V while resetting** the module, then upload.

## Bench test (one tag + home beacon radio, no ESP32 yet)

1. Flash `gateway_radio` with `LINK_ON_USB 1` and leave it connected to the PC.
2. `pip install pyserial` and run `python3 firmware/tools/gw_monitor.py /dev/ttyUSB0`.
3. Flash a tag. Open its serial port at 115200 to watch its log (`gps: fix in 3120 ms`,
   `report seq 17: ACK`, `next report in 612 s`).
4. Within about 10 s of power-up the monitor prints the first report, with an OpenStreetMap link.
5. Type `buzz 1 5`. The tag beeps after its next report, and the following report carries `cmd-done`.
6. For faster testing set `REPORT_INTERVAL_S` to 60 in `tag/config.h`.

## Home beacon: putting the two halves together

1. Flash `gateway_radio` with **`LINK_ON_USB 0`** so it talks to the ESP32 on UART1 (PB6/PB7).
   Leave `LINK_ON_USB 1` only for the PC bench test above.
2. Flash `beacon_esp32` over the ESP32-S3's USB port.
3. Power up. On a phone, join WiFi **CTv1-Beacon-xxxx** (password `ctv1setup`). The settings page
   opens; if it doesn't, browse to `http://192.168.4.1`. Choose your WiFi and enter the MQTT server
   (for a Raspberry Pi running Mosquitto: its IP, port 1883). Save.
4. The beacon restarts and joins your WiFi. Open **http://ctv1-beacon.local** to see the tags.
   (Older Android phones don't support `.local`; use the IP address shown in your router.)
5. Check the server side from any PC:

```sh
mosquitto_sub -h <server> -t 'ctv1/#' -v                                   # everything the beacon sends
mosquitto_pub -h <server> -t ctv1/gw/home/down -m '{"tag":1,"cmd":"buzz","arg":10}'
mosquitto_pub -h <server> -r -t ctv1/gw/home/fence \
  -m '{"name":"Main paddock","points":[[10.85162,76.26634],[10.8518,76.27082],[10.85018,76.27156],[10.84847,76.27119],[10.84829,76.26698]]}'
```

The fence must be sent with `-r` (retained) so the beacon gets it again after every restart.
Within a minute the web page shows the fence name and number of corners.

## Host unit tests

```sh
make -C firmware/test
```

This builds the shared library with `-Wall -Wextra -Werror` plus AddressSanitizer and UBSan,
and checks:
- AES against FIPS-197 and CMAC against RFC 4493
- packet round trips and MIC tampering
- NMEA sentences from the NMEA spec and u-blox M10 style output
- geofence, distance and bearing on the sample paddock from the UI mockup
- battery curve, report scheduling and the beacon line protocol
- MQTT JSON: report format, command and fence parsing, including bad input

## To check on real hardware

These parts compile against RUI3 4.3.0 but could not be tested without a board:
- Motion interrupts on PA0 both count (`attachInterrupt`) and wake the module from sleep (`api.system.sleep.setup`).
- Battery ADC reading with `AR_INTERNAL_3_0`. Calibrate `BATT_CALIBRATION` with a multimeter.
- ACK timing: if tags log `no ACK` while the beacon shows `UP`, raise `RX_WINDOW_MS` on the tag
  or lower `ACK_DELAY_MS` on the beacon.
- Sleep current, measured with a PPK2 or µCurrent. The target is under 50 µA with the GPS backup supply.

On the home beacon:
- Captive portal pop-up on iPhone and Android (the hotspot itself is standard ESP32 code).
- Mains-sense threshold on GPIO 1 with the real divider.
- MQTT over TLS against your broker's certificate.

## Next steps

1. The server: Mosquitto + a small backend that stores `up` messages, runs the alert rules and serves
   the app (docs/06). The UI mockup becomes its front end.
2. Store `TAG_ID`, the interval and the key in flash so one firmware build fits every collar
   (set them with AT commands).
3. u-blox UBX configuration for lower GPS power (only GGA/RMC, power-save mode).
4. Handheld beacon: listen to tags, compass arrow, buzz button.
