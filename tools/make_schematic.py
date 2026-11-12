#!/usr/bin/env python3
"""Draws the wiring schematic of the Mara X meter.

The same drawing is written as SVG (for the readme) and as an uncompressed draw.io file (to fiddle with it in
draw.io). Everything is plain Python, nothing to install.

Usage:
    python3 tools/make_schematic.py            # writes documentation/mara-x-meter.svg and .drawio
"""
from pathlib import Path
from xml.sax.saxutils import escape

WIDTH, HEIGHT = 860, 520
FONT = "Helvetica"

# ---- Parts -----------------------------------------------------------------------------------------------------
# Boxes: name, x, y, width, height, title.
BOXES = [
    ("display", 20, 100, 110, 230, "E-Ink display"),
    ("d1mini", 560, 100, 120, 230, "D1 mini"),
    ("marax", 600, 400, 120, 50, "Mara X"),
]
# Pins: (box, label, side, y). The pin line is 20 px long and sticks out of the box.
PINS = [
    ("display", "BUSY", "right", 150),
    ("display", "RST", "right", 172),
    ("display", "DC", "right", 194),
    ("display", "CS", "right", 216),
    ("display", "CLK", "right", 238),
    ("display", "DIN", "right", 260),
    ("display", "GND", "right", 282),
    ("display", "VCC", "right", 304),
    ("d1mini", "RST", "left", 150),
    ("d1mini", "A0", "left", 172),
    ("d1mini", "D0", "left", 194),
    ("d1mini", "D5", "left", 216),
    ("d1mini", "D6", "left", 238),
    ("d1mini", "D7", "left", 260),
    ("d1mini", "D8", "left", 282),
    ("d1mini", "3V3", "left", 304),
    ("d1mini", "TX", "right", 150),
    ("d1mini", "RX", "right", 172),
    ("d1mini", "D1", "right", 194),
    ("d1mini", "D2", "right", 216),
    ("d1mini", "D3", "right", 238),
    ("d1mini", "D4", "right", 260),
    ("d1mini", "GND", "right", 282),
    ("d1mini", "5V", "right", 304),
    ("marax", "TX", "right", 415),
    ("marax", "GND", "right", 435),
]

# Wires as polylines. Display pin ends are at x=150, D1 mini left pins at x=540, right pins at x=700,
# Mara X pins at x=740. The 3V3 rail runs at y=304 from the display VCC to the D1 mini 3V3, the ground rail at
# y=480.
WIRES = [
    # Display signals to the D1 mini, the three to the right hand side go over the top.
    ("BUSY-D2", [(150, 150), (200, 150), (200, 10), (750, 10), (750, 216), (700, 216)]),
    ("RST-D1", [(150, 172), (210, 172), (210, 18), (740, 18), (740, 194), (700, 194)]),
    ("DC-D3", [(150, 194), (220, 194), (220, 26), (730, 26), (730, 238), (700, 238)]),
    ("CS-D8", [(150, 216), (510, 216), (510, 282), (540, 282)]),
    ("CLK-D5", [(150, 238), (500, 238), (500, 216), (540, 216)]),
    ("DIN-D7", [(150, 260), (540, 260)]),
    # Supply rails.
    ("VCC-3V3", [(150, 304), (540, 304)]),
    ("display GND", [(150, 282), (170, 282), (170, 480), (760, 480)]),
    ("d1mini GND", [(700, 282), (760, 282), (760, 480)]),
    # Mara X: only its TX line is needed, the machine never listens.
    ("TX-D4", [(740, 415), (780, 415), (780, 260), (700, 260)]),
    ("marax GND", [(740, 435), (760, 435)]),
    # Reed sensor between 3V3 and D0, the switch symbol sits in the vertical part.
    ("D0-reed", [(540, 194), (450, 194), (450, 215)]),
    ("reed-3V3", [(450, 265), (450, 304)]),
    # Supercap with its series resistor between 3V3 and GND.
    ("3V3-R", [(240, 304), (240, 350), (300, 350)]),
    ("R-C", [(360, 350), (480, 350)]),
    ("C-GND", [(500, 350), (530, 350), (530, 480)]),
    # Pull-up on the data line, so the D1 mini boots even when the machine is off (D4 is a boot pin). The wire to
    # the 3V3 rail runs below the ground rail, the only way without crossing anything.
    ("D4-pullup", [(780, 260), (800, 260), (800, 300)]),
    ("pullup-3V3", [(800, 360), (800, 495), (160, 495), (160, 304)]),
]
JUNCTIONS = [(450, 304), (240, 304), (160, 304), (530, 480), (760, 435), (780, 260)]
# Resistors as boxes: x, y, width, height, label.
RESISTORS = [
    (300, 340, 60, 20, "3R3"),
    (790, 300, 20, 60, "4k7"),
]
# Reed switch: the contact in the vertical wire between (450, 215) and (450, 265).
REED = {"x": 450, "y0": 215, "y1": 265}
CAPACITOR = {"x": 490, "y": 350, "label": "5F"}
LABELS = [
    (465, 252, "Reed sensor", "start"),
]


def points_svg(points):
    return " ".join(f"{x},{y}" for x, y in points)


def write_svg(path: Path):
    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}" '
        f'font-family="{FONT}, Arial, sans-serif" font-size="13">',
        f'<rect width="{WIDTH}" height="{HEIGHT}" fill="white"/>',
    ]
    for _, x, y, w, h, title in BOXES:
        out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="none" stroke="black" stroke-width="1.5"/>')
        if _ == "marax":
            out.append(f'<text x="{x + 10}" y="{y + 30}" text-anchor="start" font-size="15">{escape(title)}</text>')
        else:
            out.append(f'<text x="{x + w / 2}" y="{y + 20}" text-anchor="middle" font-size="15">{escape(title)}</text>')
    box = {b[0]: b for b in BOXES}
    for b, label, side, y in PINS:
        _, x, _, w, _, _ = box[b]
        if side == "right":
            out.append(f'<line x1="{x + w}" y1="{y}" x2="{x + w + 20}" y2="{y}" stroke="black"/>')
            out.append(f'<text x="{x + w - 4}" y="{y + 4}" text-anchor="end">{escape(label)}</text>')
        else:
            out.append(f'<line x1="{x - 20}" y1="{y}" x2="{x}" y2="{y}" stroke="black"/>')
            out.append(f'<text x="{x + 4}" y="{y + 4}" text-anchor="start">{escape(label)}</text>')
    for _, points in WIRES:
        out.append(f'<polyline points="{points_svg(points)}" fill="none" stroke="black"/>')
    for x, y in JUNCTIONS:
        out.append(f'<circle cx="{x}" cy="{y}" r="3.5" fill="black"/>')
    for x, y, w, h, label in RESISTORS:
        out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="white" stroke="black"/>')
        if w > h:
            out.append(f'<text x="{x + w / 2}" y="{y + h / 2 + 4}" text-anchor="middle">{escape(label)}</text>')
        else:
            out.append(f'<text x="{x + w + 6}" y="{y + h / 2 + 4}" text-anchor="start">{escape(label)}</text>')
    # Reed switch: a contact drawn at an angle.
    rx, ry0, ry1 = REED["x"], REED["y0"], REED["y1"]
    out.append(f'<circle cx="{rx}" cy="{ry0}" r="2" fill="black"/>')
    out.append(f'<circle cx="{rx}" cy="{ry1}" r="2" fill="black"/>')
    out.append(f'<line x1="{rx}" y1="{ry1}" x2="{rx + 18}" y2="{ry0 + 8}" stroke="black" stroke-width="1.5"/>')
    # Capacitor: two plates.
    cx, cy = CAPACITOR["x"], CAPACITOR["y"]
    out.append(f'<line x1="{cx - 5}" y1="{cy - 14}" x2="{cx - 5}" y2="{cy + 14}" stroke="black" stroke-width="2"/>')
    out.append(f'<line x1="{cx + 5}" y1="{cy - 14}" x2="{cx + 5}" y2="{cy + 14}" stroke="black" stroke-width="2"/>')
    out.append(f'<text x="{cx}" y="{cy + 32}" text-anchor="middle">{escape(CAPACITOR["label"])}</text>')
    for x, y, text, anchor in LABELS:
        out.append(f'<text x="{x}" y="{y}" text-anchor="{anchor}">{escape(text)}</text>')
    out.append("</svg>")
    path.write_text("\n".join(out) + "\n")


def write_drawio(path: Path):
    cells = []
    next_id = [2]

    def add(xml):
        cells.append(xml)

    def new_id():
        next_id[0] += 1
        return str(next_id[0])

    def vertex(value, x, y, w, h, style):
        i = new_id()
        add(f'<mxCell id="{i}" value="{escape(value, {chr(10): "&lt;br&gt;"})}" style="{style}" vertex="1" parent="1">'
            f'<mxGeometry x="{x}" y="{y}" width="{w}" height="{h}" as="geometry"/></mxCell>')
        return i

    def edge(points, style):
        i = new_id()
        (x0, y0), (x1, y1) = points[0], points[-1]
        waypoints = "".join(f'<mxPoint x="{x}" y="{y}"/>' for x, y in points[1:-1])
        array = f'<Array as="points">{waypoints}</Array>' if waypoints else ""
        add(f'<mxCell id="{i}" style="{style}" edge="1" parent="1"><mxGeometry relative="1" as="geometry">'
            f'<mxPoint x="{x0}" y="{y0}" as="sourcePoint"/><mxPoint x="{x1}" y="{y1}" as="targetPoint"/>{array}'
            f'</mxGeometry></mxCell>')

    text = "text;html=1;align=center;verticalAlign=middle;resizable=0;fontFamily=Helvetica;fontSize=13;"
    box_style = "rounded=0;whiteSpace=wrap;html=1;fontSize=15;verticalAlign=top;fontFamily=Helvetica;"
    wire = "endArrow=none;html=1;endFill=0;strokeColor=#000000;"
    for _, x, y, w, h, title in BOXES:
        style = box_style if _ != "marax" else box_style.replace("verticalAlign=top", "verticalAlign=middle;align=left;spacingLeft=8")
        vertex(title, x, y, w, h, style)
    box = {b[0]: b for b in BOXES}
    for b, label, side, y in PINS:
        _, x, _, w, _, _ = box[b]
        if side == "right":
            edge([(x + w, y), (x + w + 20, y)], wire)
            vertex(label, x + w - 46, y - 10, 42, 20, text.replace("align=center", "align=right"))
        else:
            edge([(x - 20, y), (x, y)], wire)
            vertex(label, x + 4, y - 10, 40, 20, text.replace("align=center", "align=left"))
    for _, points in WIRES:
        edge(points, wire)
    for x, y in JUNCTIONS:
        vertex("", x - 3.5, y - 3.5, 7, 7, "ellipse;fillColor=#000000;strokeColor=#000000;")
    for x, y, w, h, label in RESISTORS:
        style = "rounded=0;whiteSpace=wrap;html=1;fontFamily=Helvetica;fontSize=13;"
        if w > h:
            vertex(label, x, y, w, h, style)
        else:
            vertex("", x, y, w, h, style)
            vertex(label, x + w + 2, y + h / 2 - 10, 40, 20, text.replace("align=center", "align=left"))
    rx, ry0, ry1 = REED["x"], REED["y0"], REED["y1"]
    edge([(rx, ry1), (rx + 18, ry0 + 8)], wire + "strokeWidth=1.5;")
    cx, cy = CAPACITOR["x"], CAPACITOR["y"]
    edge([(cx - 5, cy - 14), (cx - 5, cy + 14)], wire + "strokeWidth=2;")
    edge([(cx + 5, cy - 14), (cx + 5, cy + 14)], wire + "strokeWidth=2;")
    vertex(CAPACITOR["label"], cx - 20, cy + 18, 40, 20, text)
    for x, y, label, anchor in LABELS:
        align = {"start": "left", "middle": "center", "end": "right"}[anchor]
        vertex(label, x if anchor == "start" else x - 30, y - 14, 80 if anchor == "start" else 60, 20,
               text.replace("align=center", f"align={align}"))
    xml = ('<mxfile host="tools/make_schematic.py"><diagram name="Wiring" id="wiring"><mxGraphModel grid="1" '
           f'gridSize="10" pageWidth="{WIDTH}" pageHeight="{HEIGHT}"><root><mxCell id="0"/><mxCell id="1" parent="0"/>'
           + "".join(cells) + "</root></mxGraphModel></diagram></mxfile>\n")
    path.write_text(xml)


def main():
    docs = Path(__file__).resolve().parent.parent / "documentation"
    write_svg(docs / "mara-x-meter.svg")
    write_drawio(docs / "mara-x-meter.drawio")
    print(f"wrote {docs / 'mara-x-meter.svg'} and {docs / 'mara-x-meter.drawio'}")


if __name__ == "__main__":
    main()
