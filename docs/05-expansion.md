# 05 · Expansion options

Each item lists what to add and which spare pins or interfaces it uses, so the v1 PCBs
can leave room for it (footprints marked DNP = do not populate).

## Collar tag

| Expansion | Hardware | Hooks into | Value |
|---|---|---|---|
| Behaviour sensing (grazing / resting / walking / rumination) | Already have LIS3DH; add on-tag feature extraction (TinyML) | I²C, firmware | Early sickness and heat (oestrus) detection |
| Body / ambient temperature | TMP117 (±0.1 °C) on the strap, touching skin | I²C (addr 0x48) | Fever and heat-stress alerts |
| Barn / milking-shed presence | BLE beacons in the shed + scanning, needs an nRF52 or ESP32-C3 co-processor | UART2 | Indoor location where GPS fails |
| LoRaWAN mode | Same RAK3172, LoRaWAN stack instead of P2P | Firmware only | Use public networks (TTN, Helium) or a proper 8-channel gateway |
| Larger herd (> 100 tags) | Switch home beacon to an 8-channel concentrator (RAK2287 + Raspberry Pi) running LoRaWAN | Gateway swap | Multiple channels and SFs at once |
| Smaller ear-tag version | STM32WL SiP, coin-cell / small LiPo + tiny solar, 1 fix per hour | New PCB | Lighter, for calves, sheep, goats |
| Audio cue at fence line | Louder piezo driven by boost circuit | PB5 PWM | Warn the animal before it crosses (check local welfare rules first) |
| Theft / tamper | Reed switch + magnet across the buckle | Spare GPIO PB4 + EXTI | Alert if collar is opened or cut |

## Home beacon

| Expansion | Hardware | Hooks into | Value |
|---|---|---|---|
| Mobile data fallback | SIMCom A7670 / SIM7600 LTE modem | ESP32 UART2 | Alerts when the house WiFi or power is down |
| SMS alerts without internet | Same modem | ESP32 | Works on any phone |
| Extra coverage | LoRa repeater: one more RAK3172 + solar on a hill or water tank | Firmware: store-and-forward | Cover valleys and far paddocks |
| Local storage | microSD card | ESP32 SPI | Full history even with no server |
| Farm sensors on the same network | Water-trough level, gate open/closed, electric-fence voltage sensors (each a RAK3172 node) | Same packet format, new `type` | One dashboard for the farm |
| Siren / floodlight | Relay output already in v1 schematic | GPIO 6 | Scare predators / thieves at night |
| Self-hosted server on the beacon | Replace ESP32 with Raspberry Pi Zero 2 W | UART | Whole system with no external server |

## Handheld beacon

| Expansion | Hardware | Hooks into | Value |
|---|---|---|---|
| Phone link | BLE via ESP32-C3 or nRF52 module | UART2 | Use the phone map in the field with no mobile signal |
| Radio direction finding | Directional Yagi antenna on SMA, show RSSI bar | Firmware | Find a tag whose GPS is failing (RSSI "fox hunt") |
| Colour map screen | 2.4" ILI9341 TFT + offline tiles on microSD | SPI1 (PA4–PA7) | See the whole herd, not just one arrow |
| Mesh relay | Firmware mode: re-broadcast tag packets heard far from the house | Firmware | Extends range when you are out in the field |

## Software / app

- Herd analytics: daily distance walked, time grazing vs resting, favourite spots heat map.
- Pasture rotation planner using paddock fences and grazing time.
- Health records, vaccinations, breeding calendar linked to each animal.
- Multi-farm / multi-user with roles (owner, worker, vet).
- Native app (Flutter) once the PWA has proven the workflows.

Next: [06 · App requirements](06-app-requirements.md)
