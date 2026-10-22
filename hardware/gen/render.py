#!/usr/bin/env python3
"""Draws the CTv1 hardware reference set from boards.py.

    python3 hardware/gen/render.py

For each board it writes hardware/<board>/schematic.svg, pcb.svg, wiring.svg, netlist.csv and bom.csv,
plus hardware/ports.svg and hardware/README.md. Before drawing it checks every board: each net joins at
least two pins, references are unique, and each pin the firmware uses (firmware/*/config.h) is on the
net the drawing says it is.
"""
import csv
import html
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from boards import BENCH, BOARDS, PORTS, POWER_NETS  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "hardware")
FONT = "Arial, Helvetica, sans-serif"
MONO = "Consolas, 'DejaVu Sans Mono', monospace"
SIGNAL_COLOURS = ["#1e88e5", "#43a047", "#fb8c00", "#8e24aa", "#00897b", "#f4511e", "#3949ab", "#9e9d24",
                  "#6d4c41", "#d81b60", "#00acc1", "#5e35b1"]


def esc(s):
    return html.escape(str(s), quote=True)


def net_colour(net):
    if net is None:
        return "#9e9e9e"
    if net == "GND":
        return "#263238"
    if net in POWER_NETS:
        return "#c62828"
    if net in ("SDA", "SCL"):
        return "#6a1b9a"
    if re.search(r"TX|RX|LINK", net):
        return "#2e7d32"
    return "#1565c0"


def text_w(s, size=12):
    return len(s) * size * 0.6


# ---------------------------------------------------------------------------------------------
# Checks


def check(board):
    errors = []
    refs = [p.ref for p in board["parts"]]
    for r in set(refs):
        if refs.count(r) > 1:
            errors.append(f"duplicate reference {r}")
    nets = {}
    for p in board["parts"]:
        for pin, net, _ in p.pins:
            if net:
                nets.setdefault(net, []).append((p.ref, pin))
    for net, pins in nets.items():
        if len(pins) < 2:
            errors.append(f"net {net} only reaches {pins[0][0]}.{pins[0][1]}")

    # firmware pins: PIN_X <port pin or GPIO number> must sit on the expected net
    cfg = open(os.path.join(ROOT, board["firmware"])).read()
    mcu = next(p for p in board["parts"] if p.ref == ("M1" if board["id"] == "beacon" else "U1"))
    for define, want in board["fw_pins"].items():
        m = re.search(rf"#define\s+{define}\s+(\w+)", cfg)
        if not m:
            errors.append(f"{define} not found in {board['firmware']}")
            continue
        pin = m.group(1)
        label = f"GPIO{pin}" if pin.isdigit() else pin
        got = [net for name, net, _ in mcu.pins if name.split()[0] == label]
        if got != [want]:
            errors.append(f"{define} = {pin}: firmware expects net {want}, drawing has {got or 'nothing'} on {label}")
    return nets, errors


# ---------------------------------------------------------------------------------------------
# Schematic (net-label style: every pin ends in a coloured net label; same label = connected)

PIN_H, HEAD_H, STUB = 18, 46, 14


def block_size(p):
    if p.kind == "passive":
        return 96, 46
    left = [x for x in p.pins if x[2] == "L"]
    right = [x for x in p.pins if x[2] == "R"]
    w = 210 if p.kind == "chip" else 160
    return w, HEAD_H + max(len(left), len(right)) * PIN_H + 8


def label_w(net):
    return text_w(net or "nc", 11) + 14


def svg_schematic(board):
    cols = {"power": [], "mcu": [], "io": []}
    for p in board["parts"]:
        cols[p.group].append(p)
    out, x, height = [], 30, 0
    for name in ("power", "mcu", "io"):
        parts = cols[name]
        left_ext = max([label_w(n) for p in parts for _, n, s in p.pins if s == "L"] + [0]) + STUB
        right_ext = max([label_w(n) for p in parts for _, n, s in p.pins if s == "R"] + [0]) + STUB
        bw = max(block_size(p)[0] for p in parts)
        bx, y = x + left_ext, 110
        out.append(f'<text x="{x}" y="96" font-family="{FONT}" font-size="13" font-weight="700" fill="#546e7a" '
                   f'letter-spacing="1">{ {"power": "POWER", "mcu": "MCU / RADIO", "io": "PORTS AND OUTPUTS"}[name] }</text>')
        for p in parts:
            w, h = block_size(p)
            out.append(draw_block(p, bx, y, w, h))
            y += h + 22
        height = max(height, y)
        x = bx + bw + right_ext + 50
    width = x
    head = (f'<text x="30" y="40" font-family="{FONT}" font-size="24" font-weight="700" fill="#1b2418">'
            f'{esc(board["name"])} · {esc(board["title"])} · reference schematic</text>'
            f'<text x="30" y="64" font-family="{FONT}" font-size="13" fill="#546e7a">Pins with the same coloured label are '
            f'connected. Drawn from hardware/gen/boards.py, checked against {esc(board["firmware"])}. Reference only, '
            f'not checked for fabrication.</text>')
    legend, lx = [], width - 560
    for label, colour in (("GND", "#263238"), ("power", "#c62828"), ("I²C", "#6a1b9a"), ("UART", "#2e7d32"),
                          ("signal", "#1565c0")):
        legend.append(f'<rect x="{lx}" y="28" width="14" height="14" rx="3" fill="{colour}"/>'
                      f'<text x="{lx + 20}" y="40" font-family="{FONT}" font-size="12" fill="#37474f">{label}</text>')
        lx += 100
    return svg_doc(width, height + 20, head + "".join(legend) + "".join(out))


def draw_block(p, x, y, w, h):
    s = []
    if p.kind == "passive":
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="#fffde7" stroke="#8d6e63" stroke-width="1.5"/>')
        s.append(f'<text x="{x + w / 2}" y="{y + 19}" text-anchor="middle" font-family="{FONT}" font-size="13" font-weight="700" '
                 f'fill="#3e2723">{esc(p.ref)}</text>')
        s.append(f'<text x="{x + w / 2}" y="{y + 36}" text-anchor="middle" font-family="{MONO}" font-size="12" '
                 f'fill="#3e2723">{esc(p.value)}</text>')
        for _, net, side in p.pins:
            py = y + h / 2
            s.append(pin_label(x if side == "L" else x + w, py, side, net))
        return "".join(s)
    fill = "#e3f2fd" if p.kind == "chip" else "#e8f5e9"
    s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="{fill}" stroke="#455a64" stroke-width="1.5"/>')
    s.append(f'<text x="{x + 10}" y="{y + 19}" font-family="{FONT}" font-size="14" font-weight="700" fill="#102027">'
             f'{esc(p.ref)}  {esc(p.value)}</text>')
    part = p.part if len(p.part) < w / 6.2 else p.part[: int(w / 6.2) - 1] + "…"
    s.append(f'<text x="{x + 10}" y="{y + 36}" font-family="{FONT}" font-size="10.5" fill="#546e7a">{esc(part)}</text>')
    for side in ("L", "R"):
        pins = [pp for pp in p.pins if pp[2] == side]
        for i, (name, net, _) in enumerate(pins):
            py = y + HEAD_H + i * PIN_H + 9
            tx = x + 8 if side == "L" else x + w - 8
            anchor = "start" if side == "L" else "end"
            s.append(f'<text x="{tx}" y="{py + 4}" text-anchor="{anchor}" font-family="{MONO}" font-size="11" fill="#263238">{esc(name)}</text>')
            s.append(pin_label(x if side == "L" else x + w, py, side, net))
    return "".join(s)


def pin_label(px, py, side, net):
    d = -1 if side == "L" else 1
    colour = net_colour(net)
    end = px + d * STUB
    lw = label_w(net)
    lx = end - lw if side == "L" else end
    if net is None:
        return (f'<line x1="{px}" y1="{py}" x2="{end}" y2="{py}" stroke="{colour}" stroke-width="1.5"/>'
                f'<text x="{end + d * 4}" y="{py + 4}" text-anchor="{"end" if side == "L" else "start"}" font-family="{FONT}" '
                f'font-size="11" fill="{colour}">× nc</text>')
    return (f'<line x1="{px}" y1="{py}" x2="{end}" y2="{py}" stroke="{colour}" stroke-width="1.5"/>'
            f'<rect x="{lx}" y="{py - 8}" width="{lw}" height="16" rx="8" fill="{colour}"/>'
            f'<text x="{lx + lw / 2}" y="{py + 4}" text-anchor="middle" font-family="{FONT}" font-size="11" font-weight="700" '
            f'fill="#ffffff">{esc(net)}</text>')


# ---------------------------------------------------------------------------------------------
# PCB placement drawing (top view, to scale)

S = 9  # px per mm


def svg_pcb(board):
    bw, bh = board["size"]
    pcb = board["pcb"]
    edge_labels = [max(text_w(l, 11.5) for l in z[4].split("\n")) for z in pcb["zones"] if z[0] < 1]
    ox, oy = 70 + (max(edge_labels) + 10 if edge_labels else 0), 110
    W, H = bw * S, bh * S
    s = [f'<text x="30" y="40" font-family="{FONT}" font-size="24" font-weight="700" fill="#1b2418">'
         f'{esc(board["name"])} · reference PCB placement (top)</text>',
         f'<text x="30" y="64" font-family="{FONT}" font-size="13" fill="#546e7a">To scale, {bw} × {bh} mm, 2 layers. '
         f'Placement and keep-outs only: route it in KiCad from netlist.csv.</text>']
    # dimensions
    s.append(dim(ox, oy - 26, ox + W, oy - 26, f"{bw} mm", True))
    s.append(dim(ox - 30, oy, ox - 30, oy + H, f"{bh} mm", False))
    # board
    s.append(f'<rect x="{ox}" y="{oy}" width="{W}" height="{H}" rx="{2 * S}" fill="#1f5c3a" stroke="#0d3320" stroke-width="2"/>')
    for zx, zy, zw, zh, label in pcb["zones"]:
        s.append(f'<rect x="{ox + zx * S}" y="{oy + zy * S}" width="{zw * S}" height="{zh * S}" fill="url(#hatch)" '
                 f'stroke="#ef9a9a" stroke-dasharray="4 3"/>')
        lines = label.split("\n")
        tx = ox + (zx + zw) * S + 6 if zx < bw / 2 else ox + zx * S - 6
        anchor = "start" if zx < bw / 2 else "end"
        if zx < 1 or zx + zw > bw - 1:  # zone on the edge: label outside the board
            tx = ox - 8 if zx < 1 else ox + W + 8
            anchor = "end" if zx < 1 else "start"
        for i, ln in enumerate(lines):
            s.append(f'<text x="{tx}" y="{oy + zy * S + 14 + i * 14}" text-anchor="{anchor}" font-family="{FONT}" font-size="11.5" '
                     f'fill="#c62828">{esc(ln)}</text>')
    for hx, hy in pcb["holes"]:
        s.append(f'<circle cx="{ox + hx * S}" cy="{oy + hy * S}" r="{2.8 * S / 2 + 3}" fill="#c9a227"/>'
                 f'<circle cx="{ox + hx * S}" cy="{oy + hy * S}" r="{1.6 * S}" fill="#ffffff" stroke="#0d3320"/>')
    for ref, x, y, w, h, label, kind in pcb["items"]:
        s.append(pcb_item(ox + x * S, oy + y * S, w * S, h * S, ref, label, kind))
    # notes
    ny = oy + H + 50
    s.append(f'<text x="{ox}" y="{ny}" font-family="{FONT}" font-size="14" font-weight="700" fill="#1b2418">Layout notes</text>')
    for i, n in enumerate(pcb["notes"]):
        s.append(f'<text x="{ox}" y="{ny + 22 + i * 19}" font-family="{FONT}" font-size="12.5" fill="#37474f">• {esc(n)}</text>')
    width = max(ox + W + 240, ox + max(text_w(n, 12.5) for n in pcb["notes"]) + 40)
    height = ny + 22 + len(pcb["notes"]) * 19 + 20
    defs = ('<defs><pattern id="hatch" width="8" height="8" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">'
            '<rect width="8" height="8" fill="#5d1f1f" fill-opacity=".55"/><line x1="0" y1="0" x2="0" y2="8" stroke="#ef9a9a" '
            'stroke-width="2"/></pattern></defs>')
    return svg_doc(width, height, defs + "".join(s))


def dim(x1, y1, x2, y2, label, horizontal):
    mx, my = (x1 + x2) / 2, (y1 + y2) / 2
    t = (f'<text x="{mx}" y="{my - 6}" text-anchor="middle" font-family="{FONT}" font-size="12" fill="#37474f">{label}</text>'
         if horizontal else
         f'<text x="{mx - 6}" y="{my}" text-anchor="middle" font-family="{FONT}" font-size="12" fill="#37474f" '
         f'transform="rotate(-90 {mx - 6} {my})">{label}</text>')
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#78909c" stroke-width="1.2" marker-start="url(#a)" '
            f'marker-end="url(#a)"/>{t}')


def pcb_item(x, y, w, h, ref, label, kind):
    silk = "#f5f5f5"
    if kind == "module":
        return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" fill="#b0bec5" stroke="{silk}" stroke-width="1.5"/>'
                f'<text x="{x + w / 2}" y="{y + h / 2 - 2}" text-anchor="middle" font-family="{FONT}" font-size="12" font-weight="700" fill="#102027">{esc(ref)}</text>'
                f'<text x="{x + w / 2}" y="{y + h / 2 + 13}" text-anchor="middle" font-family="{FONT}" font-size="10.5" fill="#102027">{esc(label)}</text>')
    if kind == "zone_mod":
        return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" fill="none" stroke="{silk}" stroke-width="1.5" stroke-dasharray="6 4"/>'
                f'<text x="{x + w / 2}" y="{y + h / 2 + 4}" text-anchor="middle" font-family="{FONT}" font-size="11.5" fill="{silk}">{esc(ref)} · {esc(label)}</text>')
    if kind.startswith("conn"):
        n = int(kind[4])
        pins = "".join(
            (f'<rect x="{x + 2.54 * S * i + 2.54 * S / 2 - 6}" y="{y + h / 2 - 6}" width="12" height="12" fill="#c9a227"/>' if i == 0 else
             f'<circle cx="{x + 2.54 * S * i + 2.54 * S / 2}" cy="{y + h / 2}" r="6" fill="#c9a227"/>') for i in range(n))
        return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="none" stroke="{silk}" stroke-width="1.2"/>{pins}'
                f'<text x="{x}" y="{y - 5}" font-family="{FONT}" font-size="11" fill="{silk}">{esc(ref)} {esc(label)}</text>')
    if kind == "jst":
        return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="1" fill="#eceff1" stroke="{silk}"/>'
                f'<text x="{x + w / 2}" y="{y + h + 13}" text-anchor="middle" font-family="{FONT}" font-size="10.5" fill="{silk}">{esc(ref)} {esc(label)}</text>')
    if kind == "btn":
        return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="#37474f" stroke="{silk}"/>'
                f'<circle cx="{x + w / 2}" cy="{y + h / 2}" r="{w / 3}" fill="#90a4ae"/>'
                f'<text x="{x + w / 2}" y="{y + h + 13}" text-anchor="middle" font-family="{FONT}" font-size="11" fill="{silk}">{esc(ref)} {esc(label)}</text>')
    if kind == "cap":
        return (f'<circle cx="{x + w / 2}" cy="{y + h / 2}" r="{w / 2}" fill="#263238" stroke="{silk}"/>'
                f'<text x="{x + w / 2}" y="{y + h / 2 + 4}" text-anchor="middle" font-family="{FONT}" font-size="10" fill="{silk}">{esc(ref)}</text>')
    if kind == "led":
        return (f'<circle cx="{x + w / 2}" cy="{y + h / 2}" r="{w / 2 + 2}" fill="#ffcc80" stroke="{silk}"/>'
                f'<text x="{x + w + 6}" y="{y + h / 2 + 4}" font-family="{FONT}" font-size="10.5" fill="{silk}">{esc(ref)} {esc(label)}</text>')
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="#212121" stroke="{silk}"/>'
            f'<text x="{x + w / 2}" y="{y - 4}" text-anchor="middle" font-family="{FONT}" font-size="10.5" fill="{silk}">{esc(ref)}</text>')


# ---------------------------------------------------------------------------------------------
# Bench wiring (breakout boards and jumper wires)

ROW, TOP = 26, 40


def svg_wiring(key, bench, board):
    boxes, ends, bottom = {}, [], {}
    for m in sorted(bench["modules"], key=lambda m: (m[2], m[3])):
        mkey, title, x, y, side = m[:5]
        if side == "LR":
            left, right = m[5], m[6]
        else:
            left, right = (m[5], []) if side == "L" else ([], m[5])
        w = 270 if side == "LR" else 240
        h = TOP + max(len(left), len(right)) * ROW + 10
        y = max(y, bottom.get(x, -1e9) + 24)  # stack boxes in the same column without overlap
        bottom[x] = y + h
        boxes[mkey] = (title, x, y, w, h, left, right)
        for i, (pin, net) in enumerate(left):
            ends.append((mkey, "L", x, y + TOP + i * ROW + 12, pin, net))
        for i, (pin, net) in enumerate(right):
            ends.append((mkey, "R", x + w, y + TOP + i * ROW + 12, pin, net))

    hubs = ("RAK", "ESP")
    wires, colour_of, palette = [], {}, iter(SIGNAL_COLOURS * 3)
    for net in sorted({e[5] for e in ends}):
        pts = [e for e in ends if e[5] == net]
        if net not in ("GND",) and net not in POWER_NETS and net not in colour_of:
            colour_of[net] = next(palette)
        roots = [e for e in pts if e[0] == "RAK"] or [e for e in pts if e[0] in hubs] or pts[:1]
        root_mods = {r[0] for r in roots}
        for e in pts:
            if e[0] in root_mods:
                continue
            r = min(roots, key=lambda r: abs(r[2] - e[2]) + abs(r[3] - e[3]) * 0.3)
            wires.append((net, e, r))
        if len(root_mods) > 1:  # e.g. GND on both RAK and ESP: join the hubs too
            rl = sorted(roots, key=lambda r: r[2])
            wires.append((net, rl[0], rl[-1]))

    def colour(net):
        if net == "GND":
            return "#212121"
        if net in POWER_NETS:
            return "#e53935"
        return colour_of[net]

    s = [f'<text x="30" y="40" font-family="{FONT}" font-size="24" font-weight="700" fill="#1b2418">'
         f'{esc(board["name"])} · {esc(bench["title"])}: wiring with breakout boards</text>',
         f'<text x="30" y="64" font-family="{FONT}" font-size="13" fill="#546e7a">Jumper wires on a breadboard. Red = power, '
         f'black = ground, colours = signals (same colour = same wire). Same pins as the PCB, so the firmware runs unchanged.</text>']
    order = sorted(wires, key=lambda w: (w[0] not in ("GND",) and w[0] not in POWER_NETS))
    for i, (net, a, b) in enumerate(order):
        (x1, y1, s1), (x2, y2, s2) = (a[2], a[3] + 80, a[1]), (b[2], b[3] + 80, b[1])
        bend = 60 + (i % 5) * 18
        c1 = x1 + (bend if s1 == "R" else -bend)
        c2 = x2 + (bend if s2 == "R" else -bend)
        s.append(f'<path d="M{x1},{y1} C{c1},{y1} {c2},{y2} {x2},{y2}" fill="none" stroke="#ffffff" stroke-width="6" '
                 f'stroke-linecap="round"/>'
                 f'<path d="M{x1},{y1} C{c1},{y1} {c2},{y2} {x2},{y2}" fill="none" stroke="{colour(net)}" stroke-width="3.2" '
                 f'stroke-linecap="round"/>')
    height = 0
    for mkey, (title, x, y, w, h, left, right) in boxes.items():
        y += 80
        hub = mkey in hubs
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" fill="{"#e3f2fd" if hub else "#ffffff"}" '
                 f'stroke="#455a64" stroke-width="1.6"/>'
                 f'<text x="{x + 12}" y="{y + 24}" font-family="{FONT}" font-size="13.5" font-weight="700" fill="#102027">{esc(title)}</text>')
        for side, pins in (("L", left), ("R", right)):
            for i, (pin, net) in enumerate(pins):
                py = y + TOP + i * ROW + 12
                px = x if side == "L" else x + w
                tx, anchor = (x + 14, "start") if side == "L" else (x + w - 14, "end")
                s.append(f'<circle cx="{px}" cy="{py}" r="5.5" fill="{colour(net)}" stroke="#ffffff" stroke-width="1.5"/>'
                         f'<text x="{tx}" y="{py + 4}" text-anchor="{anchor}" font-family="{MONO}" font-size="11.5" fill="#263238">'
                         f'{esc(pin)}<tspan fill="#90a4ae"> {esc(net)}</tspan></text>')
        height = max(height, y + h)
    ny = height + 40
    s.append(f'<text x="30" y="{ny}" font-family="{FONT}" font-size="14" font-weight="700" fill="#1b2418">Notes</text>')
    for i, n in enumerate(bench["notes"]):
        s.append(f'<text x="30" y="{ny + 22 + i * 19}" font-family="{FONT}" font-size="12.5" fill="#37474f">• {esc(n)}</text>')
    width = max(b[1] + b[3] for b in boxes.values()) + 40
    return svg_doc(max(width, 1150), ny + 22 + len(bench["notes"]) * 19 + 20, "".join(s))


# ---------------------------------------------------------------------------------------------
# Port standard card sheet


def svg_ports():
    s = [f'<text x="30" y="40" font-family="{FONT}" font-size="24" font-weight="700" fill="#1b2418">CTv1 plug-in ports</text>',
         f'<text x="30" y="64" font-family="{FONT}" font-size="13" fill="#546e7a">Same pin order on every board, so a module '
         f'made for one board fits the others, and can be swapped for a better one later.</text>']
    cw, ch = 420, 190
    for i, (name, p) in enumerate(PORTS.items()):
        x, y = 30 + (i % 3) * (cw + 20), 90 + (i // 3) * (ch + 20)
        s.append(f'<rect x="{x}" y="{y}" width="{cw}" height="{ch}" rx="10" fill="#ffffff" stroke="#b0bec5" stroke-width="1.5"/>'
                 f'<text x="{x + 16}" y="{y + 28}" font-family="{FONT}" font-size="18" font-weight="700" fill="#102027">{esc(name)}</text>'
                 f'<text x="{x + cw - 16}" y="{y + 28}" text-anchor="end" font-family="{FONT}" font-size="11.5" fill="#546e7a">{esc(p["connector"])}</text>')
        for j, pin in enumerate(p["pins"]):
            px = x + 16 + j * 64
            s.append(f'<rect x="{px}" y="{y + 44}" width="58" height="36" rx="5" fill="{net_colour(pin if pin in ("GND",) or pin in POWER_NETS or pin in ("SDA", "SCL") else "x")}"/>'
                     f'<text x="{px + 29}" y="{y + 58}" text-anchor="middle" font-family="{FONT}" font-size="10" fill="#ffffff">{j + 1}</text>'
                     f'<text x="{px + 29}" y="{y + 73}" text-anchor="middle" font-family="{FONT}" font-size="11.5" font-weight="700" fill="#ffffff">{esc(pin)}</text>')
        for k, line in enumerate(wrap(p["use"], 62) + ["Fits: " + l if n == 0 else l for n, l in enumerate(wrap(p["fits"], 62))]):
            s.append(f'<text x="{x + 16}" y="{y + 104 + k * 16}" font-family="{FONT}" font-size="12" fill="#37474f">{esc(line)}</text>')
    return svg_doc(30 + 3 * (cw + 20) + 10, 90 + 2 * (ch + 20) + 10, "".join(s))


def wrap(t, n):
    words, lines, cur = t.split(), [], ""
    for w in words:
        if len(cur) + len(w) + 1 > n:
            lines.append(cur)
            cur = w
        else:
            cur = (cur + " " + w).strip()
    return lines + [cur] if cur else lines


def svg_doc(w, h, body):
    marker = ('<defs><marker id="a" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
              '<path d="M0,2 L10,5 L0,8 z" fill="#78909c"/></marker></defs>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{int(w)}" height="{int(h)}" viewBox="0 0 {int(w)} {int(h)}">'
            f'<rect width="100%" height="100%" fill="#fafbf7"/>{marker}{body}</svg>\n')


# ---------------------------------------------------------------------------------------------
# Netlist, BOM, README


def write_tables(board, nets, d):
    with open(os.path.join(d, "netlist.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["net", "ref", "pin"])
        for net in sorted(nets):
            for ref, pin in sorted(nets[net]):
                w.writerow([net, ref, pin])
    with open(os.path.join(d, "bom.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["ref", "value", "part", "note"])
        for p in board["parts"]:
            w.writerow([p.ref, p.value, p.part, p.note])


def readme():
    lines = ["# CTv1 hardware reference", "",
             "Reference wiring diagrams, schematics and PCB placements for the three CTv1 devices. **Generated** by",
             "`python3 hardware/gen/render.py` from one description per board (`hardware/gen/boards.py`), which also",
             "checks every net and every pin the firmware uses. Change the board file, run the script, commit the",
             "result. Nothing here is checked for fabrication yet: these are the starting point for KiCad (task T-040).", "",
             "## Modular build", "",
             "Each device is a **carrier board** with the RAK3172 and a few small parts. Everything that may change",
             "with budget or client needs plugs into a **standard port**: GPS, sensors, display, charger/battery,",
             "outputs. A cheaper or better module can be swapped in without a new board.", "",
             "![Ports](ports.svg)", "",
             "| Port | Connector | Pins | Fits |", "|---|---|---|---|"]
    for name, p in PORTS.items():
        lines.append(f"| **{name}** | {p['connector']} | {' · '.join(f'{i + 1} {x}' for i, x in enumerate(p['pins']))} | {p['fits']} |")
    lines.append("")
    for b in BOARDS:
        d = b["id"]
        lines += [f"## {b['name']}: {b['title'].lower()}", "",
                  f"Board {b['size'][0]} × {b['size'][1]} mm. Firmware pins checked against `{b['firmware']}`.", "",
                  "| Slot | Default module | How it connects | Upgrade options |", "|---|---|---|---|"]
        for slot, mod, how, up in b["modules"]:
            lines.append(f"| {slot} | {mod} | {how} | {up} |")
        lines += ["", f"**Bench wiring** (breakout boards, before the PCB):", "", f"![{d} wiring]({d}/wiring.svg)", "",
                  f"**Reference schematic**:", "", f"![{d} schematic]({d}/schematic.svg)", "",
                  f"**PCB placement**:", "", f"![{d} PCB]({d}/pcb.svg)", "",
                  f"Files: [netlist.csv]({d}/netlist.csv) · [bom.csv]({d}/bom.csv)", ""]
    lines += ["## From here to a real PCB", "",
              "1. Bench-test with breakouts using the wiring diagrams (tasks T-025 to T-028).",
              "2. Draw the schematic in KiCad from `netlist.csv`; the RAK3172 footprint comes from RAK's KiCad library.",
              "3. Place parts as in `pcb.svg`, keep the marked RF / GPS zones, pour ground on both layers.",
              "4. Run KiCad's electrical and design rule checks, then order 5 boards from any 2-layer PCB service.", ""]
    return "\n".join(lines)


def main():
    failed = False
    for b in BOARDS:
        nets, errors = check(b)
        for e in errors:
            print(f"{b['name']}: {e}")
        failed = failed or bool(errors)
        d = os.path.join(OUT, b["id"])
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, "schematic.svg"), "w").write(svg_schematic(b))
        open(os.path.join(d, "pcb.svg"), "w").write(svg_pcb(b))
        open(os.path.join(d, "wiring.svg"), "w").write(svg_wiring(b["id"], BENCH[b["id"]], b))
        write_tables(b, nets, d)
        print(f"{b['name']}: {len(b['parts'])} parts, {len(nets)} nets, {'OK' if not errors else 'ERRORS'}")
    open(os.path.join(OUT, "ports.svg"), "w").write(svg_ports())
    open(os.path.join(OUT, "README.md"), "w").write(readme())
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
