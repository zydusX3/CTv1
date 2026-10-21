# CTv1 firmware

| Folder | Runs on | Status |
|---|---|---|
| `lib/ctv1/` | everything (plain C++) | Packet format, AES-CMAC, NMEA, geofence, battery, report and GPS scheduling, settings, UBX, compass, graphics + handheld screens, MQTT JSON. **228 unit-test checks pass on a PC.** |
| `tag/` | collar tag, RAK3172 | Builds (97 KB, 48 % flash). Not yet run on hardware. |
| `gateway_radio/` | home beacon, its RAK3172 | Builds (93 KB, 46 %). Not yet run on hardware. |
| `beacon_esp32/` | home beacon, its ESP32-S3 | WiFi setup hotspot, MQTT bridge, offline buffer, backup fence alarm + siren, local web page. Builds (1.12 MB, 33 % of a 3 MB app partition). Not yet run on hardware. |
| `handheld/` | handheld beacon, RAK3172 | Arrow + distance to a chosen collar, buzz / fast-track buttons, compass calibration. Builds (102 KB, 50 %). Not yet run on hardware. |
| `tools/gw_monitor.py` | your PC | Shows decoded tag reports from the home beacon over USB and sends commands. |

The RAK3172 sketches use the RAKwireless **RUI3** Arduino core 4.3.0, built with
**Support LoRaWAN = Support LoRa P2P**. We only use LoRa P2P, and dropping the LoRaWAN stack
halves the firmware (from 171 KB to about 90 KB before our code). `beacon_esp32` uses the
Espressif **Arduino-ESP32** core 3.3.12.

## How a report works

```
tag                                         home beacon radio            handheld (if it wants this collar)
 │ RTC timer fires (10 min ± 30 s jitter)
 │ GPS only if needed (see "Battery" below)
 │── 20 B uplink (MIC) ──────────────────────►│ check MIC                 │ hears it too
 │◄──── 10 B ACK (+ queued command) ── 50 ms ─│── "UP,…" to ESP32 / PC
 │ keep listening to the end of the 1.5 s window ......................... │
 │◄──────────────────────────────── 10 B ACK + buzz/fast ── 400 ms ────────│
 │ run command, report "cmd_done" next time
 │ no ACK at all? one retry after 5–15 s, same seq (beacon re-ACKs, sends "DUP")
 │ sleep (Stop2) until the next timer or motion
```

## Battery: GPS power saving on the tag

The GPS draws ~25 mA, everything else is tiny, so the tag avoids using it
(`ctv1_gps_decide` in `lib/ctv1/src/ctv1_power.h`):

| Situation | GPS | Report |
|---|---|---|
| First fix after power-up | on, up to 180 s (cold start) | position |
| Fewer than 10 motion events since the last fix, and that fix is under 30 min old (lying down, resting) | **skipped** | last position, flag `still` |
| 3+ attempts in a row without a fix (in a shed, under a roof) | tried only every 4th report, then up to 90 s | `nofix` with last known position |
| Outside the fence, or fast-tracking | always on, up to 45 s | position |
| Otherwise | on until HDOP ≤ 2.5, up to 45 s (hot start usually 2–5 s) | position |

The first time the GPS wakes, the tag also tells it (UBX-CFG-VALSET, saved in its backup RAM)
to send only the two sentences we read, GGA and RMC.

Cattle rest or ruminate for roughly half the day, so skipping those fixes cuts GPS energy roughly in half.

## Settings stored on each device (no rebuild per collar)

The ID, farm key and radio settings are kept in the RAK3172's flash. Flash the same build onto
every collar and set each one over the programming port (115200 baud, any serial terminal):

```
ATC+CTID=7                                      collar number (tags only)
ATC+CTKEY=8f3a…(32 hex digits)                  farm key: same on every tag, beacon and handheld
ATC+CTFREQ=865062500                            radio frequency in Hz (150–960 MHz)
ATC+CTSF=9                                      spreading factor 7–12
ATC+CTPWR=14                                    transmit power, dBm
ATC+CTINT=600                                   normal report interval, seconds (tags only)
ATC+CTINFO=?                                    show everything (the key is shown only as "set")
ATZ                                             restart to use the new ID / key / radio settings
```

Make a farm key with `python3 -c "import os; print(os.urandom(16).hex())"`.
Until something is saved, each device uses the `DEFAULT_…` values in its `config.h`.
Settings are stored with a CRC; erased or corrupted flash falls back to the defaults.

## Handheld beacon

![Handheld screens](../docs/images/handheld-screens.png)

*Screens rendered on a PC from the real drawing code (`make -C firmware/test` with
`CTV1_SCREENS=<folder>`).*

| Button | Does |
|---|---|
| **<** / **>** | choose a collar (every collar heard since switch-on) |
| **OK** | buzz that collar for 30 s after its next report. Press again to cancel |
| hold **OK** | that collar reports every 2 min for 1 hour (to follow a moving animal) |
| hold **<** and **>** for 2 s | calibrate the compass: turn the handheld slowly in every direction for 20 s |

- **Arrow:** points to the collar's last position from where you stand (GPS + tilt-compensated compass).
  Until the compass is calibrated, the arrow is drawn relative to north and an **N** marks the top of the ring.
- **Vibration:** short when a new collar is heard, long when a collar reports it is outside its fence,
  medium when a buzz has been sent.
- **Mounting:** set `AXIS_*` / `SIGN_*` in `handheld/config.h` to match how the LSM303AGR sits on your board.
  Set `MAG_DECLINATION_DEG` for your area. Check with the serial log: hold it flat, top towards north, `hdg` ≈ 0.

## Set up the tools (Arduino IDE 2)

1. **File → Preferences → Additional boards manager URLs**, add
   `https://raw.githubusercontent.com/RAKWireless/RAKwireless-Arduino-BSP-Index/main/package_rakwireless.com_rui_index.json`
2. **Boards Manager** → install **RAKwireless RUI STM32 Boards** (4.3.0 or newer).
3. **Board:** *WisDuo RAK3172 Evaluation Board* (works for a bare RAK3172 on our own PCB), then
   **Tools → Support LoRaWAN → Support LoRa P2P**. With the default "LoRaWAN and LoRa P2P" the
   handheld does not fit.
4. Install the shared library. Either copy or link `firmware/lib/ctv1` into your
   `Arduino/libraries` folder, or zip that folder and use **Sketch → Include Library → Add .ZIP Library**.

With arduino-cli:

```sh
FQBN=rak_rui:stm32:WisDuoRAK3172EvaluationBoard:supportlora=3
arduino-cli compile -b $FQBN --library firmware/lib/ctv1 firmware/tag
arduino-cli compile -b $FQBN --library firmware/lib/ctv1 firmware/gateway_radio
arduino-cli compile -b $FQBN --library firmware/lib/ctv1 firmware/handheld
```

### ESP32-S3 (home beacon)

1. **Boards Manager** → install **esp32 by Espressif Systems** (3.x).
2. **Library Manager** → install **PubSubClient** by Nick O'Leary. Also install `firmware/lib/ctv1` as above.
3. **Board:** *ESP32S3 Dev Module*. For an ESP32-S3-WROOM-1 **N8R2**: Flash Size **8MB**,
   Partition Scheme **8M with spiffs (3MB APP/1.5MB SPIFFS)**, PSRAM **QSPI PSRAM**, USB CDC On Boot **Enabled**.

```sh
arduino-cli compile -b espressif:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=enabled \
  --library firmware/lib/ctv1 firmware/beacon_esp32
```

## Flashing

The RAK3172 is programmed through **UART2 (PA2/PA3)** with a 3.3 V USB-serial adapter
(TX→PA3, RX→PA2, GND, 3V3). Upload from the IDE as normal; RUI3 resets the module into its
bootloader with an AT command. The same port gives the `ATC+` settings commands and the log.

When `gateway_radio` runs with `LINK_ON_USB 1`, the AT commands on that port are switched
off. To upload again, pull **BOOT0 to 3.3 V while resetting** the module, then upload.

## Bench test (one tag + home beacon radio, no ESP32 yet)

1. Flash `gateway_radio` with `LINK_ON_USB 1` and leave it connected to the PC.
   (Set its key/frequency first with a `LINK_ON_USB 0` build, or keep the defaults on every device.)
2. `pip install pyserial` and run `python3 firmware/tools/gw_monitor.py /dev/ttyUSB0`.
3. Flash a tag. Open its serial port at 115200 to watch its log (`gps: fix in 3120 ms`,
   `report seq 17: ACK`, `next report in 612 s`). For quicker testing: `ATC+CTINT=60`.
4. Within about 10 s of power-up the monitor prints the first report, with an OpenStreetMap link.
5. Type `buzz 1 5`. The tag beeps after its next report, and the following report carries `cmd-done`.
6. Leave the tag still on a table: after the first fix, reports show `still` and the GPS stays off.

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
CTV1_SCREENS=/tmp/screens ./firmware/test/test_ctv1   # also writes the handheld screens as .pbm images
```

This builds the shared library with `-Wall -Wextra -Werror` plus AddressSanitizer and UBSan,
and checks:
- AES against FIPS-197 and CMAC against RFC 4493
- packet round trips and MIC tampering
- NMEA sentences from the NMEA spec and u-blox M10 style output
- geofence, distance and bearing on the sample paddock from the UI mockup
- battery curve, report scheduling and the GPS skip/back-off rules
- settings blob (CRC-32 check value, corrupted and erased flash) and every `ATC+` value check
- UBX framing against a known checksum (MON-VER poll)
- compass heading on simulated tilted readings (±1°), hard-iron calibration, small trig against libm
- screen drawing, and MQTT JSON report / command / fence parsing including bad input

## To check on real hardware

These parts compile against RUI3 4.3.0 but could not be tested without a board:
- Motion interrupts on PA0 both count (`attachInterrupt`) and wake the module from sleep (`api.system.sleep.setup`).
- Battery ADC reading with `AR_INTERNAL_3_0`. Calibrate `BATT_CALIBRATION` with a multimeter.
- ACK timing: if tags log `no ACK` while the beacon shows `UP`, raise `RX_WINDOW_MS` on the tag
  or lower `ACK_DELAY_MS` on the beacon. Handheld commands land at `HANDHELD_ACK_DELAY_MS` (400 ms).
- The u-blox CFG-VALSET keys for the MAX-M10S (GGA/RMC on, others off): check the log goes quiet apart from GGA/RMC.
- LSM303AGR axis directions on your PCB, and the SH1106 column offset (set `OLED_SH1106 0` for an SSD1306).
- `ATC+` commands under RUI3's AT parser.
- Sleep current, measured with a PPK2 or µCurrent. The target is under 50 µA with the GPS backup supply.

On the home beacon:
- Captive portal pop-up on iPhone and Android (the hotspot itself is standard ESP32 code).
- Mains-sense threshold on GPIO 1 with the real divider.
- MQTT over TLS against your broker's certificate.

## Next steps

1. The server: Mosquitto + a small backend that stores `up` messages, runs the alert rules and serves
   the app (docs/06). The UI mockup becomes its front end.
2. Field test with one animal: range from the roof antenna, GPS fix times on the collar, battery per day.
