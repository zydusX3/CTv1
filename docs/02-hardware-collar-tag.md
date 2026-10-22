# 02 · Collar tag hardware

> Pin numbers below are the STM32 port names on the RAK3172. Check every pin against the
> current RAK3172 datasheet before making a PCB. Pins that the RAK AT firmware uses
> (UART2 = PA2/PA3) are left free for programming and debug.

## Schematic

```
                         POWER
 ┌──────────────┐  SS14        ┌───────────────────────┐        ┌────────────────────┐
 │ Solar panel  │──►|──┬──────►│ VIN   CN3791 module   │  BAT+  │ 18650 3.4 Ah cell  │
 │ 6 V  1 W     │      │       │ (MPPT set ≈ 5.2 V)    │───┬───►│ + DW01/8205A prot. │
 └──────────────┘      │       │ GND                   │   │    └─────────┬──────────┘
 USB-C 5V (service)────┘       └───────────────────────┘   │              │ BAT−
                                                           │             GND
                       VBAT ───────────────────────────────┤
                                                           │
           ┌───────────────────────────────────────────────┼─────────────────────────┐
           │                                               │                         │
      ┌────┴────┐                                     R1 1 MΩ                        │
      │ MCP1700 │ 3V3                                      │                         │
 VBAT─┤IN    OUT├──┬────┬─────► 3V3 rail                   ├──────► BAT_SENSE (PB3)  │
      │   GND   │  │    │                                  │    C 100 nF to GND      │
      └────┬────┘ 1µF  10µF + 220µF low-ESR             R2 1 MΩ                      │
          GND     (bulk cap covers the TX current spike)   │                         │
                                                          GND                        │

                         MCU + RADIO  (RAK3172 = STM32WLE5CC)
                    ┌──────────────────────────────────────────┐
     3V3 ──┬────────┤ VDD                                  RF  ├──── u.FL ── 868/915 MHz antenna
        100nF 10µF  │                                          │     (flexible whip or helical,
           GND ─────┤ GND                                      │      run it up the collar strap)
                    │                                          │
   GPS_TX ─────────►│ PB7  UART1_RX                            │
   GPS_RX ◄─────────│ PB6  UART1_TX                            │
                    │                                          │
   GPS_EN ◄─────────│ PA8  (high = GPS powered)                │
                    │                                          │
   I2C_SDA ◄───────►│ PA11 I2C2_SDA ─── 4.7 kΩ to 3V3          │
   I2C_SCL ◄────────│ PA12 I2C2_SCL ─── 4.7 kΩ to 3V3          │
   ACC_INT1 ───────►│ PA0  (EXTI wake from Stop2)              │
                    │                                          │
   BAT_SENSE ──────►│ PB3  ADC                                 │
   LED ◄────────────│ PA1  ── 1 kΩ ──►|── GND   (red 0603)     │
   BUZZ ◄───────────│ PB5  (to buzzer driver below)            │
                    │                                          │
  Debug/program ◄──►│ PA2 UART2_TX / PA3 UART2_RX  ── 4-pin pad │
  SWD ◄────────────►│ PA13 SWDIO / PA14 SWCLK / NRST ── 5-pin  │
                    │ BOOT0 ── 10 kΩ to GND                    │
                    └──────────────────────────────────────────┘

                         GPS
          ┌─────────────┐
 3V3 ─────┤ IN   TPS22917├─ GPS_VCC ──┬──────────┐
 GPS_EN ──┤ ON       OUT │          10µF         │
          │ GND          │           GND         │
          └─────────────┘            ┌───────────┴─────────────┐
                                     │ VCC     u-blox MAX-M10S │
 3V3 ─── 100 Ω ──┬───────────────────┤ V_BCKP  (keeps almanac  │
               10 µF                 │          → hot start)   │
                GND                  │ TXD ──────────► GPS_TX  │
                                     │ RXD ◄────────── GPS_RX  │
                                     │ RF_IN ─── 25×25 mm      │
                                     │           ceramic patch │
                                     │ GND                     │
                                     └─────────────────────────┘
   Patch antenna must face the sky: mount the electronics box on TOP of the neck,
   and use the battery (bottom of the collar) as a counterweight so it stays there.

                         ACCELEROMETER
                     ┌────────────────────┐
 3V3 ──┬─────────────┤ VDD, VDD_IO  LIS3DH│
     100nF           │ CS ── 3V3 (I²C mode)
      GND ───────────┤ GND, SDO ── GND (addr 0x18)
 I2C_SDA ◄──────────►│ SDA                │
 I2C_SCL ───────────►│ SCL                │
 ACC_INT1 ◄──────────│ INT1 (motion wake) │
                     └────────────────────┘

                         BUZZER (find mode)
 3V3 ───────┬──────────────┐
            │         ┌────┴────┐
       1N4148 (flyback)  Magnetic buzzer 3 V, 2.7 kHz
            │         └────┬────┘
            └──────────────┤
                           D
 BUZZ (PB5) ── 100 Ω ──── G   2N7002
                    100 kΩ │
                     GND   S ── GND
 (drive PB5 with a 2.7 kHz PWM from a timer for the loudest tone)
```

## Bill of materials (one tag)

| Qty | Part | Notes | Approx. cost (USD) |
|---|---|---|---|
| 1 | RAK3172 (or Seeed Wio-E5) for your band, with IPEX connector | STM32WLE5CC + LoRa | 7–10 |
| 1 | u-blox MAX-M10S module **or** cheaper ATGM336H / Quectel L76K board | MAX-M10S has the lowest power | 4–15 |
| 1 | 25×25 mm passive GPS patch antenna | if the GPS board has none | 2 |
| 1 | 868/915 MHz flexible antenna, IPEX | | 2 |
| 1 | LIS3DH breakout or chip | motion + wake | 1–3 |
| 1 | MCP1700T-3302E/TT LDO | | 0.4 |
| 1 | TPS22917 load switch (or an SI2301 P-MOSFET) | GPS power gate | 0.5 |
| 1 | CN3791 solar MPPT charger module | | 1.5 |
| 1 | 6 V 1 W solar panel (≈ 110×60 mm) | optional, strongly advised | 3 |
| 1 | 18650 3.4 Ah protected cell (Panasonic NCR18650B or similar) | | 4–6 |
| 1 | 3 V magnetic buzzer + 2N7002 + 1N4148 | | 0.5 |
| — | Caps, resistors, LED, SS14 diode, headers | | 1 |
| 1 | IP67 enclosure (≈ 100×68×50 mm), cable glands, collar strap 50 mm nylon | | 6–10 |
| | **Total** | | **≈ 35–60** |

## Firmware outline

```
main loop (bare metal or STM32Cube + LoRa P2P / RadioLib):
  on RTC alarm or ACC_INT1:
      jitter = random(0..30 s); sleep(jitter)
      GPS_EN = 1; read NMEA/UBX until fix or 45 s timeout; GPS_EN = 0
      read battery (ADC), accel activity count
      build 20-byte packet, add MIC; LoRa TX
      open RX window 1 s; if ACK: handle command (buzz / interval)
      else: retry once after 5–15 s random
      choose next interval:
          10 min normal | 2 min if outside fence or fast-track | 30 min if batt < 20 %
      enter Stop2
```

## Mechanical notes

- Put the GPS patch and solar panel on top of the neck, the battery underneath as counterweight.
- Pot or conformal-coat the PCB. Cattle rub collars on fences and trees.
- Use a 2-part stainless buckle; avoid anything that can snag. Leave two fingers of slack.
- Weight target: under 350 g total.

Next: [03 · Home beacon hardware](03-hardware-home-beacon.md)
