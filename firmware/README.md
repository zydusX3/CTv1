# CTv1 firmware

| Folder | Runs on | Status |
|---|---|---|
| `lib/ctv1/` | everything (plain C++) | Packet format, AES-CMAC, NMEA parser, geofence, battery, scheduling, beacon line protocol. **117 unit-test checks pass on a PC.** |
| `tag/` | collar tag, RAK3172 | Builds (88 % flash). Not yet run on hardware. |
| `gateway_radio/` | home beacon, its RAK3172 | Builds (87 % flash). Not yet run on hardware. |
| `tools/gw_monitor.py` | your PC | Shows decoded tag reports from the home beacon over USB and sends commands. |
| ESP32-S3 WiFi/MQTT side of the home beacon | | Next |
| Handheld beacon | | Later |

Both sketches are built with the RAKwireless **RUI3** Arduino core 4.3.0 for the RAK3172.
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

## To check on real hardware

These parts compile against RUI3 4.3.0 but could not be tested without a board:
- Motion interrupts on PA0 both count (`attachInterrupt`) and wake the module from sleep (`api.system.sleep.setup`).
- Battery ADC reading with `AR_INTERNAL_3_0`. Calibrate `BATT_CALIBRATION` with a multimeter.
- ACK timing: if tags log `no ACK` while the beacon shows `UP`, raise `RX_WINDOW_MS` on the tag
  or lower `ACK_DELAY_MS` on the beacon.
- Sleep current, measured with a PPK2 or µCurrent. The target is under 50 µA with the GPS backup supply.

## Next steps

1. ESP32-S3 side of the home beacon: WiFi setup page, MQTT publish of `UP` lines, commands from MQTT,
   offline buffer (docs/03).
2. Store `TAG_ID`, the interval and the key in flash so one firmware build fits every collar
   (set them with AT commands).
3. u-blox UBX configuration for lower GPS power (only GGA/RMC, power-save mode).
4. Handheld beacon: listen to tags, compass arrow, buzz button.
