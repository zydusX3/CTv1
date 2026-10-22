#!/usr/bin/env python3
"""Bench tool for the CTv1 home beacon radio (gateway_radio.ino with LINK_ON_USB 1).

Shows every tag report decoded, and lets you send commands to tags.

    pip install pyserial
    python3 gw_monitor.py /dev/ttyUSB0        (Windows: COM5)
    python3 gw_monitor.py - < saved_log.txt   (decode a saved log, no serial port)

Commands you can type while it runs:
    buzz <tag> [seconds]      make a collar beep (default 30 s)
    fast <tag> <hours>        report every 2 min for N hours (0 = stop)
    interval <tag> <minutes>  normal report interval
    ping
"""
import struct
import sys
import threading
import time

FLAGS = ["fix", "moving", "low-batt", "charging", "outside", "cmd-done", "still"]
COMMANDS = {"buzz": 1, "interval": 2, "fast": 3}


def decode_uplink(pkt: bytes) -> dict:
    """Decode a 20-byte CTv1 uplink (MIC already checked by the radio). Layout: docs/01."""
    if len(pkt) != 20 or pkt[0] not in (1, 2):
        raise ValueError("not a CTv1 uplink")
    typ, tag, seq, lat, lon, batt, flags, sh, act = struct.unpack_from("<BHBiiBBBB", pkt)
    return {
        "type": "position" if typ == 1 else "no-fix",
        "tag": tag,
        "seq": seq,
        "lat": lat / 1e7,
        "lon": lon / 1e7,
        "batt_v": (2000 + batt * 10) / 1000,
        "flags": [name for i, name in enumerate(FLAGS) if flags & (1 << i)],
        "sats": sh >> 4,
        "hdop": (sh & 0x0F) / 2,
        "activity": act,
    }


def describe(line: str) -> str:
    parts = line.strip().split(",")
    kind = parts[0]
    stamp = time.strftime("%H:%M:%S")
    if kind == "UP" and len(parts) == 4:
        u = decode_uplink(bytes.fromhex(parts[3]))
        where = f"{u['lat']:.6f}, {u['lon']:.6f}" if u["lat"] or u["lon"] else "no position yet"
        osm = f"https://www.openstreetmap.org/?mlat={u['lat']:.6f}&mlon={u['lon']:.6f}#map=18/{u['lat']:.6f}/{u['lon']:.6f}"
        return (f"{stamp}  tag {u['tag']:>3}  seq {u['seq']:>3}  {where}  {u['sats']} sats hdop {u['hdop']:.1f}  "
                f"{u['batt_v']:.2f} V  act {u['activity']:>3}  {' '.join(u['flags'])}  "
                f"rssi {parts[1]} snr {parts[2]}\n          {osm}")
    if kind == "ST" and len(parts) == 5:
        return f"{stamp}  beacon up {int(parts[1]) // 60} min, {parts[2]} packets, {parts[3]} rejected, {parts[4]} commands queued"
    if kind == "BAD":
        return f"{stamp}  rejected packet (wrong key or not ours): {line.strip()}"
    return f"{stamp}  {line.strip()}"


def to_command(text: str):
    words = text.split()
    if not words:
        return None
    if words[0] == "ping":
        return "PING"
    if words[0] in COMMANDS and len(words) >= 2:
        arg = int(words[2]) if len(words) > 2 else (30 if words[0] == "buzz" else 0)
        return f"CMD,{int(words[1])},{COMMANDS[words[0]]},{arg}"
    raise ValueError("try: buzz 7 30 | fast 7 3 | interval 7 10 | ping")


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    if sys.argv[1] == "-":
        for line in sys.stdin:
            print(describe(line))
        return

    import serial  # pyserial

    port = serial.Serial(sys.argv[1], 115200, timeout=1)

    def reader():
        while True:
            raw = port.readline().decode(errors="replace")
            if raw.strip():
                print(describe(raw), flush=True)

    threading.Thread(target=reader, daemon=True).start()
    print(f"listening on {sys.argv[1]}; type 'buzz <tag>' etc., Ctrl+C to quit")
    for text in sys.stdin:
        try:
            cmd = to_command(text)
            if cmd:
                port.write((cmd + "\n").encode())
        except ValueError as e:
            print(e)


if __name__ == "__main__":
    main()
