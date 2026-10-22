"""CTv1 reference boards: one description per board, used to draw every diagram.

Every board is a small carrier with the same plug-in ports, so modules can be swapped or
upgraded without a new PCB (GPS module, sensor breakout, display, charger/battery, outputs).
Pin names on the RAK3172 are STM32 port names (PA8, PB6...). Pad numbers differ between
RAK3172 variants: check them in the RAK3172 datasheet when drawing the footprint.
"""

# ---------------------------------------------------------------------------------------------
# Building blocks


class Part:
    """A component. pins: list of (pin name, net or None for not connected, side 'L'/'R')."""

    def __init__(self, ref, value, part="", group="io", pins=(), note=""):
        self.ref, self.value, self.part, self.group, self.note = ref, value, part, group, note
        self.pins = list(pins)

    @property
    def kind(self):
        if self.ref[0] in "RC" and len(self.pins) == 2:
            return "passive"
        if self.ref.startswith("J"):
            return "conn"
        return "chip"


def R(ref, value, a, b, group, note=""):
    return Part(ref, value, "Resistor 0603", group, [("1", a, "L"), ("2", b, "R")], note)


CAP_PARTS = {"100n": "100 nF X7R 0603", "1u": "1 µF X7R 0603", "10u": "10 µF X5R 0805", "22u": "22 µF X5R 0805",
             "220u": "220 µF 6.3 V low-ESR polymer/tantalum"}


def C(ref, value, a, b, group, note=""):
    return Part(ref, value, CAP_PARTS[value], group, [("1", a, "L"), ("2", b, "R")], note)


def port(ref, port_type, nets, group="io", note=""):
    """A CTv1 standard port: pins numbered in the order of PORTS[port_type]."""
    names = PORTS[port_type]["pins"]
    assert len(names) == len(nets), (ref, port_type)
    return Part(ref, port_type, PORTS[port_type]["connector"], group,
                [(f"{i + 1} {n}", net, "L") for i, (n, net) in enumerate(zip(names, nets))], note)


# The plug-in ports shared by all boards. Same pin order everywhere, so a module made for one board fits the others.
PORTS = {
    "GPS-6": {"connector": "1x6 2.54 mm header", "pins": ["VGPS", "GND", "TXD", "RXD", "VBCKP", "PPS"],
              "use": "GPS module. TXD = module output to the MCU. VGPS is switched; VBCKP is always on (hot start).",
              "fits": "u-blox MAX-M10S breakout (default) · ATGM336H · Quectel L76K · later: u-blox M10 with LNA, dual-band"},
    "SNS-5": {"connector": "1x5 2.54 mm header", "pins": ["3V3", "GND", "SDA", "SCL", "INT"],
              "use": "I²C sensor with one interrupt line.",
              "fits": "LIS3DH (tag) · LSM303AGR (handheld) · later: TMP117 temperature, LIS2DW12, BME280"},
    "DSP-4": {"connector": "1x4 2.54 mm header", "pins": ["GND", "3V3", "SCL", "SDA"],
              "use": "I²C display, in the pin order most OLED modules use.",
              "fits": "1.3\" SH1106 (default) · 0.96\" SSD1306 · later: other I²C OLEDs"},
    "BAT-2": {"connector": "JST-PH 2-pin", "pins": ["VBAT", "GND"],
              "use": "Battery side of a charger module. The board never charges by itself.",
              "fits": "CN3791 solar MPPT + 18650 (tag) · TP4056 USB-C + LiPo (handheld) · IP5306 UPS (beacon) · later: bigger cells, BQ25570 harvester"},
    "DBG-6": {"connector": "1x6 2.54 mm header", "pins": ["3V3", "GND", "RX2", "TX2", "BOOT0", "NRST"],
              "use": "Programming, ATC+ settings and log (RAK3172 UART2). RX2 goes to the adapter's TXD.",
              "fits": "any 3.3 V USB-serial adapter (CP2102, CH340, FT232)"},
    "OUT-2": {"connector": "JST-PH 2-pin", "pins": ["V+", "SW"],
              "use": "Low-side switched output (MOSFET + flyback diode on the board).",
              "fits": "magnetic buzzer · coin vibration motor · LED strip · later: louder piezo with booster"},
}

# Net names that are power rails (drawn red) and the ground net.
POWER_NETS = {"VBAT", "3V3", "VGPS", "VBCKP", "V5", "VIN5", "V12", "VSW"}

# ---------------------------------------------------------------------------------------------
# Boards


def rak3172(group, io):
    """RAK3172 with power/debug pins on the left and the board's IO on the right."""
    pins = [("VDD", "3V3", "L"), ("GND", "GND", "L"), ("PA2 / TX2", "TX2", "L"), ("PA3 / RX2", "RX2", "L"),
            ("BOOT0", "BOOT0", "L"), ("RST", "NRST", "L"), ("PA13 SWDIO", "SWDIO", "L"), ("PA14 SWCLK", "SWCLK", "L"),
            ("RF (IPEX)", None, "L")]
    pins += [(p, n, "R") for p, n in io]
    return Part("U1", "RAK3172", "RAK3172 (STM32WLE5CC + LoRa), IPEX antenna variant, band per region (IN865 default)",
                group, pins, "Antenna on the module's IPEX connector.")


def swd_pads():
    return Part("TP1", "SWD pads", "4 test pads (or Tag-Connect TC2030): recovery / debugger", "mcu",
                [("3V3", "3V3", "L"), ("SWDIO", "SWDIO", "L"), ("SWCLK", "SWCLK", "L"), ("GND", "GND", "L")])


def tag_board():
    parts = [
        port("J1", "BAT-2", ["VBAT", "GND"], "power", "From the CN3791 solar charger, which holds the 18650."),
        C("C1", "1u", "VBAT", "GND", "power"),
        Part("U2", "MCP1700-3302", "MCP1700T-3302E/TT, 3.3 V LDO, Iq 1.6 µA", "power",
             [("VIN", "VBAT", "L"), ("GND", "GND", "L"), ("VOUT", "3V3", "R")]),
        C("C2", "220u", "3V3", "GND", "power", "Low-ESR bulk: covers the 20 dBm TX burst."),
        C("C5", "10u", "3V3", "GND", "power"),
        R("R1", "1M", "VBAT", "BATT_SENSE", "power", "Battery divider, 2 µA."),
        R("R2", "1M", "BATT_SENSE", "GND", "power"),
        C("C3", "100n", "BATT_SENSE", "GND", "power"),
        Part("U3", "TPS22917", "TPS22917DBVR load switch (or SI2301 P-MOSFET)", "power",
             [("VIN", "3V3", "L"), ("ON", "GPS_EN", "L"), ("GND", "GND", "L"), ("VOUT", "VGPS", "R"), ("CT", None, "R")]),
        C("C6", "10u", "VGPS", "GND", "power"),
        R("R6", "100", "3V3", "VBCKP", "power", "GPS backup supply: keeps the almanac for 2-5 s hot starts."),
        C("C4", "10u", "VBCKP", "GND", "power"),
        rak3172("mcu", [("PB7 / RX1", "GPS_TXD"), ("PB6 / TX1", "GPS_RXD"), ("PA8", "GPS_EN"), ("PA11 SDA", "SDA"),
                        ("PA12 SCL", "SCL"), ("PA0", "ACC_INT"), ("PB3 (ADC)", "BATT_SENSE"), ("PA1", "LED"),
                        ("PB5", "BUZZ")]),
        C("C7", "100n", "3V3", "GND", "mcu", "Next to the RAK3172 VDD pad."),
        R("R3", "10k", "BOOT0", "GND", "mcu"),
        R("R4", "4.7k", "SDA", "3V3", "mcu"),
        swd_pads(),
        R("R5", "4.7k", "SCL", "3V3", "mcu"),
        port("J2", "GPS-6", ["VGPS", "GND", "GPS_TXD", "GPS_RXD", "VBCKP", None]),
        port("J3", "SNS-5", ["3V3", "GND", "SDA", "SCL", "ACC_INT"], note="LIS3DH breakout, address 0x18."),
        port("J4", "DBG-6", ["3V3", "GND", "RX2", "TX2", "BOOT0", "NRST"]),
        R("R7", "1k", "LED", "LED_A", "io"),
        Part("D1", "LED red", "LED 0603", "io", [("A", "LED_A", "L"), ("K", "GND", "R")]),
        R("R8", "100", "BUZZ", "BUZZ_G", "io"),
        R("R9", "100k", "BUZZ_G", "GND", "io"),
        Part("Q1", "2N7002", "2N7002 N-MOSFET SOT-23", "io", [("G", "BUZZ_G", "L"), ("S", "GND", "L"), ("D", "BUZZ_SW", "R")]),
        Part("D2", "1N4148", "1N4148W flyback", "io", [("A", "BUZZ_SW", "L"), ("K", "3V3", "R")]),
        port("J5", "OUT-2", ["3V3", "BUZZ_SW"], note="Magnetic buzzer 3 V, 2.7 kHz."),
    ]
    return {
        "id": "tag", "name": "CTv1-TAG", "title": "Collar tag carrier", "size": (60, 40),
        "firmware": "firmware/tag/config.h", "parts": parts,
        "fw_pins": {"PIN_GPS_EN": "GPS_EN", "PIN_ACC_INT": "ACC_INT", "PIN_BATT": "BATT_SENSE", "PIN_LED": "LED",
                    "PIN_BUZZER": "BUZZ"},
        "pcb": {
            "holes": [(3.5, 3.5), (56.5, 3.5), (3.5, 36.5), (56.5, 36.5)],
            "items": [
                ("U1", 4, 14, 16, 16, "RAK3172", "module"),
                ("J2", 20, 33, 15.24, 2.54, "GPS-6", "conn6h"),
                ("J3", 39, 33, 12.7, 2.54, "SNS-5", "conn5h"),
                ("J4", 26, 4, 15.24, 2.54, "DBG-6", "conn6h"),
                ("J1", 50, 6, 6, 4.5, "BAT-2", "jst"),
                ("J5", 50, 14, 6, 4.5, "OUT-2", "jst"),
                ("U2", 28, 12, 3, 3, "LDO", "ic"),
                ("U3", 33, 12, 3, 3, "SW", "ic"),
                ("C2", 28, 18, 6, 6, "220µ", "cap"),
                ("Q1", 44, 16, 3, 3, "Q1", "ic"),
                ("D1", 44, 22, 2, 1.2, "LED", "ic"),
            ],
            "zones": [(0, 9, 3.5, 26, "Antenna cable exit\n(IPEX on U1)")],
            "notes": [
                "2 layers, 1.6 mm FR4, solid ground pour both sides, stitching vias every 5 mm.",
                "Mount on TOP of the neck: GPS module (on J2) faces the sky, battery hangs below as counterweight.",
                "Keep the GPS module and its patch antenna away from the RAK3172 RF cable.",
                "C2 (220 µF) within 10 mm of U1 VDD; C7 at the pad.",
                "Fits a 100×68×50 mm IP67 box with the 18650 and charger beside it. Conformal coat after test.",
            ],
        },
        "modules": [
            ("U1", "RAK3172 (IN865)", "on the board", "same footprint: RAK3172-SiP, RAK3172LP"),
            ("J1", "CN3791 MPPT module + 18650 3.4 Ah + 6 V 1 W panel", "plugs into BAT-2", "bigger panel/cell; BQ25570 harvester"),
            ("J2", "u-blox MAX-M10S breakout", "plugs into GPS-6", "ATGM336H (cheaper), L76K"),
            ("J3", "LIS3DH breakout", "plugs into SNS-5", "LIS2DW12 (lower power), + TMP117 on the same bus"),
            ("J5", "3 V magnetic buzzer", "plugs into OUT-2", "louder piezo, vibration"),
        ],
    }


def beacon_board():
    parts = [
        port("J1", "BAT-2", ["VIN5", "GND"], "power", "5 V adapter in (BAT-2 type connector reused as DC in)."),
        Part("M2", "IP5306 UPS", "IP5306 charge + 5 V boost module with 18650 (about 12 h backup)", "power",
             [("IN+", "VIN5", "L"), ("GND", "GND", "L"), ("OUT+", "V5", "R")]),
        R("R1", "100k", "VIN5", "VBUS_SENSE", "power", "Mains present? 5 V reads ~2.5 V."),
        R("R2", "100k", "VBUS_SENSE", "GND", "power"),
        Part("M1", "ESP32-S3-DevKitC-1", "ESP32-S3-DevKitC-1 N8R2 on 2x22 sockets (its 3V3 regulator also feeds U1)", "mcu",
             [("5V", "V5", "L"), ("GND", "GND", "L"), ("3V3 (out)", "3V3", "L"), ("GPIO0 BOOT", None, "L"),
              ("GPIO18 RX1", "LINK_RX", "R"), ("GPIO17 TX1", "LINK_TX", "R"), ("GPIO16", "NRST", "R"),
              ("GPIO15", "RADIO_IRQ", "R"), ("GPIO1 ADC", "VBUS_SENSE", "R"), ("GPIO4", "LED_RX", "R"),
              ("GPIO5", "LED_ALARM", "R"), ("GPIO6", "SIREN_EN", "R"), ("GPIO8 SDA", "SDA", "R"), ("GPIO9 SCL", "SCL", "R")]),
        rak3172("mcu", [("PB6 / TX1", "LINK_RX"), ("PB7 / RX1", "LINK_TX"), ("PA0", "RADIO_IRQ")]),
        C("C1", "10u", "3V3", "GND", "mcu"),
        C("C2", "100n", "3V3", "GND", "mcu"),
        R("R9", "10k", "BOOT0", "GND", "mcu"),
        swd_pads(),
        R("R7", "4.7k", "SDA", "3V3", "mcu"),
        R("R8", "4.7k", "SCL", "3V3", "mcu"),
        port("J5", "DSP-4", ["GND", "3V3", "SCL", "SDA"], note="Optional status OLED (not driven yet)."),
        port("J6", "DBG-6", ["3V3", "GND", "RX2", "TX2", "BOOT0", "NRST"], note="RAK3172 programming / ATC+ settings."),
        R("R3", "330", "LED_RX", "LED1_A", "io"),
        Part("D1", "LED green", "LED 3 mm", "io", [("A", "LED1_A", "L"), ("K", "GND", "R")]),
        R("R4", "330", "LED_ALARM", "LED2_A", "io"),
        Part("D2", "LED red", "LED 3 mm", "io", [("A", "LED2_A", "L"), ("K", "GND", "R")]),
        R("R5", "220", "SIREN_EN", "SIREN_G", "io"),
        R("R6", "100k", "SIREN_G", "GND", "io"),
        Part("Q1", "AO3400", "AO3400A logic-level N-MOSFET, 5 A", "io", [("G", "SIREN_G", "L"), ("S", "GND", "L"), ("D", "SIREN_SW", "R")]),
        Part("D3", "SS14", "SS14 Schottky flyback", "io", [("A", "SIREN_SW", "L"), ("K", "V12", "R")]),
        port("J3", "OUT-2", ["V12", "SIREN_SW"], note="12 V siren or relay coil."),
        Part("J4", "12 V in", "2-pin screw terminal", "io", [("1 V12", "V12", "L"), ("2 GND", "GND", "L")],
             "Only needed for a 12 V siren."),
    ]
    return {
        "id": "beacon", "name": "CTv1-BEACON", "title": "Home beacon carrier", "size": (100, 70),
        "firmware": "firmware/beacon_esp32/config.h", "parts": parts,
        "fw_pins": {"PIN_RADIO_RX": "LINK_RX", "PIN_RADIO_TX": "LINK_TX", "PIN_RADIO_RESET": "NRST",
                    "PIN_VBUS_SENSE": "VBUS_SENSE", "PIN_LED_RX": "LED_RX", "PIN_LED_ALARM": "LED_ALARM",
                    "PIN_SIREN": "SIREN_EN"},
        "pcb": {
            "holes": [(4, 4), (96, 4), (4, 66), (96, 66)],
            "items": [
                ("M1", 30, 18, 63, 28, "ESP32-S3-DevKitC-1 (2x22 sockets)", "module"),
                ("U1", 8, 22, 16, 16, "RAK3172", "module"),
                ("M2", 8, 50, 40, 14, "IP5306 UPS + 18650 (off-board)", "zone_mod"),
                ("J1", 52, 58, 7, 5, "5V IN", "jst"),
                ("J4", 64, 58, 7, 5, "12V", "jst"),
                ("J3", 76, 58, 7, 5, "SIREN", "jst"),
                ("J6", 8, 8, 15.24, 2.54, "DBG-6", "conn6h"),
                ("J5", 30, 8, 10.16, 2.54, "DSP-4", "conn4h"),
                ("D1", 88, 52, 3, 3, "RX", "led"),
                ("D2", 88, 58, 3, 3, "ALM", "led"),
                ("Q1", 76, 51, 3, 3, "Q1", "ic"),
            ],
            "zones": [(0, 24, 4, 12, "u.FL pigtail →\nN-type bulkhead,\nroof antenna")],
            "notes": [
                "2 layers, 1.6 mm. The DevKitC plugs in, so it can be replaced or upgraded (e.g. ESP32-S3 with Ethernet).",
                "Mount the box under the roof near the antenna; keep the coax (LMR-240) under 5 m, with the lightning arrester grounded.",
                "Keep the DevKitC's WiFi antenna end clear of the ground pour and metal (board edge).",
                "Siren traces (J3/J4/Q1) 1 mm wide; screw terminals at the board edge.",
            ],
        },
        "modules": [
            ("U1", "RAK3172 (IN865)", "on the board", "Heltec V3 instead of U1 + M1 for quick tests"),
            ("M1", "ESP32-S3-DevKitC-1 N8R2", "2x22 sockets", "ESP32-S3 + W5500 Ethernet; + A7670 LTE on a spare UART"),
            ("M2", "IP5306 UPS module + 18650", "wired to J1 / V5", "bigger cell, 12 V UPS"),
            ("J3", "12 V siren / relay", "plugs into OUT-2", "floodlight relay"),
            ("J5", "0.96\" SSD1306 OLED", "plugs into DSP-4", "1.3\" SH1106"),
        ],
    }


def handheld_board():
    parts = [
        port("J1", "BAT-2", ["VBAT", "GND"], "power", "From the TP4056 USB-C charger module, which holds the LiPo."),
        Part("SW1", "Power", "SPDT slide switch", "power", [("COM", "VBAT", "L"), ("ON", "VSW", "R")]),
        C("C1", "1u", "VSW", "GND", "power"),
        Part("U2", "MCP1700-3302", "MCP1700T-3302E/TT, 3.3 V LDO", "power",
             [("VIN", "VSW", "L"), ("GND", "GND", "L"), ("VOUT", "3V3", "R")]),
        C("C2", "22u", "3V3", "GND", "power"),
        R("R1", "1M", "VSW", "BATT_SENSE", "power", "Divider after the switch: no drain when off."),
        R("R2", "1M", "BATT_SENSE", "GND", "power"),
        C("C3", "100n", "BATT_SENSE", "GND", "power"),
        Part("U3", "TPS22917", "TPS22917DBVR load switch", "power",
             [("VIN", "3V3", "L"), ("ON", "GPS_EN", "L"), ("GND", "GND", "L"), ("VOUT", "VGPS", "R"), ("CT", None, "R")]),
        C("C6", "10u", "VGPS", "GND", "power"),
        R("R6", "100", "3V3", "VBCKP", "power"),
        C("C4", "10u", "VBCKP", "GND", "power"),
        rak3172("mcu", [("PB7 / RX1", "GPS_TXD"), ("PB6 / TX1", "GPS_RXD"), ("PA8", "GPS_EN"), ("PA11 SDA", "SDA"),
                        ("PA12 SCL", "SCL"), ("PA0", "BTN_PREV"), ("PA1", "BTN_OK"), ("PB4", "BTN_NEXT"),
                        ("PB5", "VIB"), ("PA10", "BUZZ"), ("PB3 (ADC)", "BATT_SENSE")]),
        C("C7", "100n", "3V3", "GND", "mcu"),
        R("R3", "10k", "BOOT0", "GND", "mcu"),
        R("R4", "4.7k", "SDA", "3V3", "mcu"),
        swd_pads(),
        R("R5", "4.7k", "SCL", "3V3", "mcu"),
        Part("SW2", "<", "6 mm tactile, IP67 if possible", "io", [("1", "BTN_PREV", "L"), ("2", "GND", "R")]),
        Part("SW3", "OK", "6 mm tactile", "io", [("1", "BTN_OK", "L"), ("2", "GND", "R")]),
        Part("SW4", ">", "6 mm tactile", "io", [("1", "BTN_NEXT", "L"), ("2", "GND", "R")]),
        port("J2", "GPS-6", ["VGPS", "GND", "GPS_TXD", "GPS_RXD", "VBCKP", None]),
        port("J3", "SNS-5", ["3V3", "GND", "SDA", "SCL", None], note="LSM303AGR breakout (0x19 / 0x1E)."),
        port("J4", "DSP-4", ["GND", "3V3", "SCL", "SDA"], note="1.3\" SH1106 OLED (0x3C)."),
        port("J5", "DBG-6", ["3V3", "GND", "RX2", "TX2", "BOOT0", "NRST"]),
        R("R8", "100", "VIB", "VIB_G", "io"),
        R("R9", "100k", "VIB_G", "GND", "io"),
        Part("Q1", "2N7002", "2N7002 N-MOSFET", "io", [("G", "VIB_G", "L"), ("S", "GND", "L"), ("D", "VIB_SW", "R")]),
        Part("D2", "1N4148", "1N4148W flyback", "io", [("A", "VIB_SW", "L"), ("K", "3V3", "R")]),
        port("J6", "OUT-2", ["3V3", "VIB_SW"], note="10 mm coin vibration motor."),
        R("R10", "100", "BUZZ", "BUZZ_G", "io"),
        R("R11", "100k", "BUZZ_G", "GND", "io"),
        Part("Q2", "2N7002", "2N7002 N-MOSFET", "io", [("G", "BUZZ_G", "L"), ("S", "GND", "L"), ("D", "BUZZ_SW", "R")]),
        Part("D3", "1N4148", "1N4148W flyback", "io", [("A", "BUZZ_SW", "L"), ("K", "3V3", "R")]),
        port("J7", "OUT-2", ["3V3", "BUZZ_SW"], note="Magnetic buzzer."),
    ]
    return {
        "id": "handheld", "name": "CTv1-HAND", "title": "Handheld beacon carrier", "size": (100, 55),
        "firmware": "firmware/handheld/config.h", "parts": parts,
        "fw_pins": {"PIN_GPS_EN": "GPS_EN", "PIN_BTN_PREV": "BTN_PREV", "PIN_BTN_OK": "BTN_OK", "PIN_BTN_NEXT": "BTN_NEXT",
                    "PIN_VIB": "VIB", "PIN_BUZZER": "BUZZ", "PIN_BATT": "BATT_SENSE"},
        "pcb": {
            "holes": [(3.5, 3.5), (96.5, 3.5), (3.5, 51.5), (96.5, 51.5)],
            "items": [
                ("J4", 30, 6, 40, 30, "OLED 1.3\" (on DSP-4, above board)", "zone_mod"),
                ("SW2", 32, 44, 6, 6, "<", "btn"),
                ("SW3", 47, 44, 6, 6, "OK", "btn"),
                ("SW4", 62, 44, 6, 6, ">", "btn"),
                ("U1", 8, 18, 16, 16, "RAK3172", "module"),
                ("J2", 76, 6, 15.24, 2.54, "GPS-6", "conn6h"),
                ("J3", 76, 44, 12.7, 2.54, "SNS-5", "conn5h"),
                ("J5", 8, 44, 15.24, 2.54, "DBG-6", "conn6h"),
                ("J1", 88, 22, 6, 4.5, "BAT", "jst"),
                ("SW1", 88, 33, 8, 4, "PWR", "ic"),
                ("J6", 76, 22, 6, 4.5, "VIB", "jst"),
                ("J7", 76, 30, 6, 4.5, "BUZ", "jst"),
                ("U2", 10, 8, 3, 3, "LDO", "ic"),
                ("U3", 16, 8, 3, 3, "SW", "ic"),
            ],
            "zones": [(96.5, 8, 3.5, 10, "GPS antenna:\ntop edge,\nfaces up")],
            "notes": [
                "2 layers, 1.6 mm. OLED sits on spacers above the board, buttons below it (thumb reach).",
                "Compass breakout on J3 at least 20 mm from the vibration motor, buzzer and battery (magnetic parts).",
                "GPS module at the top edge, antenna facing up when held. SMA bulkhead on the case top for the LoRa antenna.",
                "Fits a 115×65×25 mm handheld case with a 103450 LiPo under the board.",
            ],
        },
        "modules": [
            ("U1", "RAK3172 (IN865)", "on the board", "same footprint: RAK3172-SiP"),
            ("J1", "TP4056 USB-C module (with protection) + LiPo 1200 mAh", "plugs into BAT-2", "bigger LiPo"),
            ("J2", "u-blox MAX-M10S breakout", "plugs into GPS-6", "ATGM336H, L76K"),
            ("J3", "LSM303AGR breakout", "plugs into SNS-5", "LIS2MDL + LSM6DSO (better compass)"),
            ("J4", "1.3\" SH1106 OLED", "plugs into DSP-4", "0.96\" SSD1306 (set OLED_SH1106 0)"),
            ("J6/J7", "coin vibration motor / buzzer", "plug into OUT-2", "louder buzzer"),
        ],
    }


BOARDS = [tag_board(), beacon_board(), handheld_board()]

# ---------------------------------------------------------------------------------------------
# Bench wiring with off-the-shelf breakouts (before any PCB exists)

BENCH = {
    "tag": {
        "title": "Collar tag on the bench",
        "modules": [
            ("PWR", "18650 + TP4056 + 3.3 V LDO board", 40, 60, "R",
             [("VBAT", "VBAT"), ("3V3", "3V3"), ("GND", "GND")]),
            ("USB", "USB-serial 3.3 V (CP2102)", 40, 330, "R",
             [("GND", "GND"), ("TXD", "RX2"), ("RXD", "TX2")]),
            ("RAK", "RAK3172 breakout", 430, 60, "LR",
             [("VDD", "3V3"), ("GND", "GND"), ("PA3 RX2", "RX2"), ("PA2 TX2", "TX2"), ("BOOT0", "BOOT0"), ("PB3", "BATT_SENSE")],
             [("PB7 RX1", "GPS_TXD"), ("PB6 TX1", "GPS_RXD"), ("PA11 SDA", "SDA"), ("PA12 SCL", "SCL"), ("PA0", "ACC_INT"),
              ("PA1", "LED"), ("PB5", "BUZZ"), ("GND", "GND"), ("VDD", "3V3")]),
            ("DIV", "Divider 1M + 1M + 100 nF", 40, 200, "R",
             [("top", "VBAT"), ("mid", "BATT_SENSE"), ("bottom", "GND")]),
            ("JMP", "Jumper: BOOT0 to GND", 40, 460, "R", [("BOOT0", "BOOT0"), ("GND", "GND")]),
            ("GPS", "MAX-M10S breakout", 860, 40, "L",
             [("VCC", "3V3"), ("GND", "GND"), ("TX", "GPS_TXD"), ("RX", "GPS_RXD")]),
            ("ACC", "LIS3DH breakout", 860, 210, "L",
             [("VIN", "3V3"), ("GND", "GND"), ("SDA", "SDA"), ("SCL", "SCL"), ("INT1", "ACC_INT")]),
            ("LED", "LED + 1 kΩ", 860, 400, "L", [("+", "LED"), ("-", "GND")]),
            ("BZ", "Buzzer + 2N7002 + 1N4148", 860, 490, "L", [("IN", "BUZZ"), ("3V3", "3V3"), ("GND", "GND")]),
        ],
        "notes": [
            "Power the RAK3172 from the battery/LDO board, not from the USB-serial adapter (TX at 20 dBm draws ~90 mA).",
            "On the bench the GPS is powered all the time; PA8 (GPS_EN) can stay unconnected.",
            "Connect only GND, TXD and RXD from the USB-serial adapter.",
        ],
    },
    "beacon": {
        "title": "Home beacon on the bench",
        "modules": [
            ("USB", "USB-serial 3.3 V (for ATC+ settings)", 40, 60, "R",
             [("GND", "GND"), ("TXD", "RX2"), ("RXD", "TX2")]),
            ("JMP", "Jumper: BOOT0 to GND", 40, 220, "R", [("BOOT0", "BOOT0"), ("GND", "GND")]),
            ("RAK", "RAK3172 breakout", 430, 60, "LR",
             [("PA3 RX2", "RX2"), ("PA2 TX2", "TX2"), ("BOOT0", "BOOT0"), ("GND", "GND")],
             [("VDD", "3V3"), ("GND", "GND"), ("PB6 TX1", "LINK_RX"), ("PB7 RX1", "LINK_TX"), ("RST", "NRST"), ("PA0", "RADIO_IRQ")]),
            ("ESP", "ESP32-S3-DevKitC-1 N8R2", 860, 40, "L",
             [("3V3", "3V3"), ("GND", "GND"), ("GPIO18", "LINK_RX"), ("GPIO17", "LINK_TX"), ("GPIO16", "NRST"),
              ("GPIO15", "RADIO_IRQ"), ("GPIO1", "VBUS_SENSE"), ("GPIO4", "LED_RX"), ("GPIO5", "LED_ALARM"), ("5V", "V5")]),
            ("DIV", "Divider 100k + 100k on 5 V", 430, 420, "R", [("top", "V5"), ("mid", "VBUS_SENSE"), ("bottom", "GND")]),
            ("LEDS", "2 LEDs + 330 Ω", 430, 540, "R", [("green", "LED_RX"), ("red", "LED_ALARM"), ("GND", "GND")]),
        ],
        "notes": [
            "Power everything from the DevKitC's USB port: its 3V3 pin feeds the RAK3172.",
            "Flash gateway_radio with LINK_ON_USB 0 so it talks to the ESP32 on PB6/PB7.",
            "On the bench the divider measures the DevKitC's own 5V pin (always 'mains').",
        ],
    },
    "handheld": {
        "title": "Handheld on the bench",
        "modules": [
            ("PWR", "LiPo + TP4056 + 3.3 V LDO board", 40, 60, "R",
             [("VBAT", "VSW"), ("3V3", "3V3"), ("GND", "GND")]),
            ("DIV", "Divider 1M + 1M + 100 nF", 40, 200, "R", [("top", "VSW"), ("mid", "BATT_SENSE"), ("bottom", "GND")]),
            ("BTN", "3 push buttons to GND", 40, 330, "R",
             [("<", "BTN_PREV"), ("OK", "BTN_OK"), (">", "BTN_NEXT"), ("GND", "GND")]),
            ("USB", "USB-serial 3.3 V", 40, 490, "R", [("GND", "GND"), ("TXD", "RX2"), ("RXD", "TX2")]),
            ("RAK", "RAK3172 breakout", 430, 60, "LR",
             [("VDD", "3V3"), ("GND", "GND"), ("PB3", "BATT_SENSE"), ("PA0", "BTN_PREV"), ("PA1", "BTN_OK"),
              ("PB4", "BTN_NEXT"), ("PA3 RX2", "RX2"), ("PA2 TX2", "TX2")],
             [("PB7 RX1", "GPS_TXD"), ("PB6 TX1", "GPS_RXD"), ("PA11 SDA", "SDA"), ("PA12 SCL", "SCL"), ("PB5", "VIB"),
              ("PA10", "BUZZ"), ("GND", "GND"), ("VDD", "3V3")]),
            ("GPS", "MAX-M10S breakout", 860, 40, "L",
             [("VCC", "3V3"), ("GND", "GND"), ("TX", "GPS_TXD"), ("RX", "GPS_RXD")]),
            ("CMP", "LSM303AGR breakout", 860, 210, "L", [("VIN", "3V3"), ("GND", "GND"), ("SDA", "SDA"), ("SCL", "SCL")]),
            ("OLED", "1.3\" SH1106 OLED", 860, 360, "L", [("GND", "GND"), ("VCC", "3V3"), ("SCL", "SCL"), ("SDA", "SDA")]),
            ("VIB", "Motor + 2N7002 + diode", 860, 510, "L", [("IN", "VIB"), ("3V3", "3V3"), ("GND", "GND")]),
            ("BZ", "Buzzer + 2N7002 + diode", 860, 630, "L", [("IN", "BUZZ"), ("3V3", "3V3"), ("GND", "GND")]),
        ],
        "notes": [
            "Buttons need no resistors: the firmware turns on the internal pull-ups.",
            "Keep the compass breakout a hand's width from the motor, buzzer and battery.",
            "The OLED and compass share SDA/SCL; most breakouts already have pull-up resistors.",
        ],
    },
}
