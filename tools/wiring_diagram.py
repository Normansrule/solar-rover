"""Draw the wiring diagrams in docs/img/ from the firmware pin maps.

    python3 tools/wiring_diagram.py

Reads the GPIO numbers from firmware/car/src/config.h and
firmware/remote/src/main.cpp, so the pictures can't drift from the code.
Writes wiring_car.svg, wiring_remote.svg and wiring_power.svg. Pins are grouped
by what they connect to, not by where they sit on the DevKit; the board's
silkscreen shows where each GPIO is.
"""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
IMG = ROOT / "docs" / "img"

COL = {"pwr": "#d62728", "gnd": "#222222", "sig": "#c9a100", "sda": "#1f77b4",
       "scl": "#2ca02c", "33": "#ff7f0e", "uart": "#9467bd", "motor": "#8c564b"}
FONT = "font-family='DejaVu Sans, Arial, sans-serif'"


def pins(path):
    src = path.read_text()
    out = {}
    for m in re.finditer(r"PIN_(\w+)\s*=\s*(\d+)", src):
        out[m.group(1)] = int(m.group(2))
    return out


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def text(x, y, s, size=13, anchor="start", weight="normal", fill="#111"):
    return (f"<text x='{x}' y='{y}' {FONT} font-size='{size}' text-anchor='{anchor}' "
            f"font-weight='{weight}' fill='{fill}'>{esc(s)}</text>")


def harness(title, board, left, right, note):
    """left/right: list of (module name, [(board pin label, module pin label, colour key)])."""
    row, gap, top = 22, 30, 110
    def height(groups):
        return sum(len(r) * row + gap for _, r in groups)
    h_rows = max(height(left), height(right))
    W, bx, bw = 1180, 470, 240
    H = top + h_rows + 90
    out = [f"<svg xmlns='http://www.w3.org/2000/svg' width='{W}' height='{H}' viewBox='0 0 {W} {H}'>",
           f"<rect width='{W}' height='{H}' fill='white'/>",
           text(W / 2, 34, title, 22, "middle", "bold"),
           text(W / 2, 56, note, 12, "middle", fill="#555"),
           f"<rect x='{bx}' y='{top - 40}' width='{bw}' height='{h_rows + 40}' rx='10' fill='#1b3a5c' stroke='#0d2238' stroke-width='2'/>",
           text(bx + bw / 2, top - 16, board, 17, "middle", "bold", "white")]
    for side, groups in (("L", left), ("R", right)):
        y = top
        for name, rows in groups:
            gh = len(rows) * row
            mx = 40 if side == "L" else W - 40 - 190
            out.append(f"<rect x='{mx}' y='{y - 4}' width='190' height='{gh + 4}' rx='6' fill='#f3f3f3' stroke='#777'/>")
            out.append(text(mx + 95, y - 9, name, 13, "middle", "bold"))
            for i, (bp, mp, c) in enumerate(rows):
                yy = y + i * row + row / 2
                if bp.startswith("@"):   # not an ESP32 pin: short stub with a label
                    c0 = COL[c]
                    if side == "L":
                        x1, x2, tx, an = mx + 190, mx + 215, mx + 220, "start"
                        out.append(text(mx + 8, yy + 4, mp, 12))
                    else:
                        x1, x2, tx, an = mx, mx - 25, mx - 30, "end"
                        out.append(text(mx + 182, yy + 4, mp, 12, "end"))
                    out.append(f"<line x1='{x1}' y1='{yy}' x2='{x2}' y2='{yy}' stroke='{c0}' stroke-width='3'/>")
                    out.append(text(tx, yy + 4, bp[1:], 11, an, fill="#444"))
                    continue
                if side == "L":
                    x1, x2 = mx + 190, bx
                    out.append(text(mx + 8, yy + 4, mp, 12))
                    out.append(text(bx + 8, yy + 4, bp, 12, fill="white"))
                else:
                    x1, x2 = bx + bw, mx
                    out.append(text(mx + 182, yy + 4, mp, 12, "end"))
                    out.append(text(bx + bw - 8, yy + 4, bp, 12, "end", fill="white"))
                out.append(f"<line x1='{x1}' y1='{yy}' x2='{x2}' y2='{yy}' stroke='{COL[c]}' stroke-width='3'/>")
                for xx in (x1, x2):
                    out.append(f"<circle cx='{xx}' cy='{yy}' r='3.5' fill='{COL[c]}'/>")
            y += gh + gap
    ly = H - 30
    lx = 40
    for k, lab in (("pwr", "+ supply"), ("gnd", "GND"), ("33", "3.3 V"), ("sda", "I2C SDA"), ("scl", "I2C SCL"),
                   ("sig", "signal"), ("uart", "UART"), ("motor", "motor")):
        out.append(f"<line x1='{lx}' y1='{ly}' x2='{lx + 26}' y2='{ly}' stroke='{COL[k]}' stroke-width='4'/>")
        out.append(text(lx + 32, ly + 4, lab, 12))
        lx += 120
    out.append("</svg>")
    return "\n".join(out)


def car():
    p = pins(ROOT / "firmware/car/src/config.h")
    g = lambda k: f"GPIO{p[k]}"
    left = [
        ("TB6612 left (FL + RL)", [
            ("@MAIN+ rail", "VM", "pwr"), ("3V3", "VCC", "33"), ("GND", "GND", "gnd"),
            (g("MOTOR_STBY"), "STBY", "sig"),
            (g("L_IN1"), "AIN1 + BIN1", "sig"), (g("L_IN2"), "AIN2 + BIN2", "sig"),
            (g("FL_PWM"), "PWMA", "sig"), (g("RL_PWM"), "PWMB", "sig"),
            ("@front-left motor", "AO1 / AO2", "motor"), ("@rear-left motor", "BO1 / BO2", "motor")]),
        ("TB6612 right (FR + RR)", [
            ("@MAIN+ rail", "VM", "pwr"), ("3V3", "VCC", "33"), ("GND", "GND", "gnd"),
            (g("MOTOR_STBY"), "STBY", "sig"),
            (g("R_IN1"), "AIN1 + BIN1", "sig"), (g("R_IN2"), "AIN2 + BIN2", "sig"),
            (g("FR_PWM"), "PWMA", "sig"), (g("RR_PWM"), "PWMB", "sig"),
            ("@front-right motor", "AO1 / AO2", "motor"), ("@rear-right motor", "BO1 / BO2", "motor")]),
        ("LM393 encoders ×4 (3V3, GND)", [
            (g("ENC_FL"), "front-left DO", "sig"), (g("ENC_RL"), "rear-left DO", "sig"),
            (g("ENC_FR") + " (VP)", "front-right DO", "sig"), (g("ENC_RR") + " (VN)", "rear-right DO", "sig")]),
    ]
    right = [
        ("Power in (expansion board 5V block)", [("5V", "5V lever connector", "pwr"), ("GND", "GND lever connector", "gnd")]),
        ("Passive I2C hub (one port per board)", [
            (g("SDA"), "SDA", "sda"), (g("SCL"), "SCL", "scl"), ("3V3", "VCC", "33"), ("GND", "GND", "gnd"),
            ("@PCA9685 0x40", "", "sda"), ("@MPU-6050 0x68", "", "sda"), ("@VL53L0X 0x29", "", "sda"),
            ("@INA219 batt 0x41", "", "sda"), ("@INA219 solar 0x44", "", "sda")]),
        ("Raspberry Pi Zero 2 W", [
            (g("PI_RX") + " (RX2)", "pin 8  GPIO14 TXD", "uart"), (g("PI_TX") + " (TX2)", "pin 10 GPIO15 RXD", "uart"),
            (g("PI_WAKE"), "pin 5  GPIO3", "sig"), ("@5.1 V rail", "pin 2  5V", "pwr"), ("GND", "pin 6  GND", "gnd")]),
        ("DS18B20 probe adapter", [(g("TEMP"), "DAT", "sig"), ("3V3", "VCC", "33"), ("GND", "GND", "gnd")]),
        ("LoRa radio E220-900T22D", [(g("LORA_MODE"), "M0 + M1", "sig"), (g("LORA_TX") + " (TX1)", "RXD", "uart"),
                                     (g("LORA_RX") + " (RX1)", "TXD", "uart"), ("3V3", "VCC", "33"), ("GND", "GND", "gnd")]),
        ("Charger relay module (jumper H)", [(g("CHG_EN"), "IN", "sig"), ("@5V lever connector", "VCC", "pwr"), ("GND", "GND", "gnd")]),
        ("PCA9685 servo outputs", [("@base yaw MG996R", "ch 0", "sig"), ("@shoulder MG996R", "ch 1", "sig"),
                                   ("@elbow MG996R", "ch 2", "sig"), ("@wrist MG90S", "ch 3", "sig"),
                                   ("@gripper MG90S", "ch 4", "sig"), ("@6.0 V rail (XL4015)", "V+ terminal", "pwr")]),
    ]
    return harness("Solar Rover: ESP32 wiring", "ESP32 DevKit 38-pin",
                   left, right, "Generated from firmware/car/src/config.h. Pins grouped by module, not by board position.")


def remote():
    p = pins(ROOT / "firmware/remote/src/main.cpp")
    g = lambda k: f"GPIO{p[k]}"
    left = [
        ("Left joystick KY-023", [("3V3", "+5V pin (use 3V3!)", "33"), ("GND", "GND", "gnd"),
                                  (g("LX"), "VRx", "sig"), (g("LY"), "VRy", "sig"), (g("LCLICK"), "SW", "sig")]),
        ("Right joystick KY-023", [("3V3", "+5V pin (use 3V3!)", "33"), ("GND", "GND", "gnd"),
                                   (g("RX"), "VRx", "sig"), (g("RY"), "VRy", "sig"), (g("RCLICK"), "SW", "sig")]),
    ]
    right = [
        ("Buttons (other leg to GND)", [(g("A"), "A: gripper", "sig"), (g("B"), "B: arm shift", "sig")]),
        ("0-25 V voltage sensor module", [(g("VBAT"), "S", "sig"), ("GND", "−", "gnd"), ("@TP4056 B+ / B−", "VCC / GND in", "pwr")]),
        ("Power (MT3608 5.0 V out)", [("VIN", "MT3608 OUT+", "pwr"), ("GND", "MT3608 OUT−", "gnd")]),
        ("LoRa radio E220-900T22D", [(g("LORA_MODE"), "M0 + M1", "sig"), (g("LORA_TX") + " (TX2)", "RXD", "uart"),
                                     (g("LORA_RX") + " (RX2)", "TXD", "uart"), ("3V3", "VCC", "33"), ("GND", "GND", "gnd")]),
    ]
    return harness("Handheld remote: ESP32 wiring", "ESP32 DevKit",
                   left, right, "Generated from firmware/remote/src/main.cpp. 18650 → TP4056 → switch → MT3608 (5.0 V) → VIN.")


def power():
    W, H = 1180, 700
    o = [f"<svg xmlns='http://www.w3.org/2000/svg' width='{W}' height='{H}' viewBox='0 0 {W} {H}'>",
         f"<rect width='{W}' height='{H}' fill='white'/>",
         "<defs><marker id='a' viewBox='0 0 10 10' refX='9' refY='5' markerWidth='11' markerHeight='11' markerUnits='userSpaceOnUse' orient='auto'>"
         "<path d='M0,0 L10,5 L0,10 z' fill='#333'/></marker></defs>",
         text(W / 2, 34, "Solar Rover: power and charging (modules, no board)", 22, "middle", "bold"),
         text(W / 2, 56, "All grounds are common. Thick lines carry motor/servo current (18 AWG).", 12, "middle", fill="#555")]
    box = {}

    def b(k, x, y, w, h, title, sub="", fill="#f3f3f3"):
        box[k] = (x, y, w, h)
        o.append(f"<rect x='{x}' y='{y}' width='{w}' height='{h}' rx='7' fill='{fill}' stroke='#555' stroke-width='1.5'/>")
        o.append(text(x + w / 2, y + (h / 2 if not sub else h / 2 - 4), title, 13, "middle", "bold"))
        if sub:
            o.append(text(x + w / 2, y + h / 2 + 13, sub, 11, "middle", fill="#444"))

    def wire(pts, label="", c="pwr", wgt=3, lab_at=0, arrow=True):
        d = " ".join(f"{'M' if i == 0 else 'L'}{x},{y}" for i, (x, y) in enumerate(pts))
        m = " marker-end='url(#a)'" if arrow else ""
        o.append(f"<path d='{d}' fill='none' stroke='{COL[c]}' stroke-width='{wgt}' stroke-linejoin='round'{m}/>")
        if label:
            (x1, y1), (x2, y2) = pts[lab_at], pts[lab_at + 1]
            o.append(text((x1 + x2) / 2, (y1 + y2) / 2 - 6, label, 11, "middle", fill="#333"))

    # charge path (top row)
    b("sol", 30, 90, 150, 50, "Solar 2× 6 V 2 W", "in series, 12 V", "#fff6d5")
    b("isol", 230, 90, 130, 50, "INA219 solar", "0x44")
    b("d1", 410, 90, 80, 50, "D1 SS54")
    b("jack", 30, 190, 150, 50, "12 V DC jack", "5.5 × 2.1 mm", "#fff6d5")
    b("d2", 410, 190, 80, 50, "D2 SS54")
    b("q1", 560, 140, 150, 50, "Relay module", "COM → NO")
    b("chg", 770, 140, 150, 50, "CN3795 MPPT", "8.40 V, ≤1.5 A")
    b("ibat", 980, 140, 160, 50, "INA219 batt", "0x41")
    # pack and main rail
    b("pack", 980, 280, 160, 70, "4× 18650 2S2P", "+ 2S BMS (P+ / P−)", "#e5f2e5")
    b("fuse", 770, 290, 150, 50, "Fuse 7.5 A", "+ XT30")
    b("sw", 560, 290, 150, 50, "Main switch")
    b("main", 330, 290, 170, 50, "MAIN+ rail", "6.6–8.4 V", "#fde2e2")
    b("tbl", 30, 400, 170, 50, "TB6612 left VM")
    b("tbr", 30, 470, 170, 50, "TB6612 right VM")
    b("xl", 260, 430, 170, 50, "XL4015 buck", "set 6.0 V")
    b("pca", 260, 540, 170, 50, "PCA9685 V+", "+ 1000 µF, 5 servos")
    b("mp", 490, 430, 170, 50, "MP1584 buck", "set 5.10 V")
    b("esp", 460, 540, 110, 50, "ESP32 5V")
    b("pi", 590, 540, 110, 50, "Pi pin 2")
    # gate drive detail
    b("gate", 770, 420, 370, 170, "", fill="#f7f7ff")
    gx, gy = 785, 445
    for i, line in enumerate(["No soldered board: Wago 221 lever connectors",
                              "MAIN+, GND, 5V, CHG_IN and PACK+ are each one",
                              "lever connector (wiring.md section 5).",
                              "Relay module: VCC = 5V, IN = ESP32 GPIO4, jumper H.",
                              "GPIO4 HIGH = charging allowed (0–45 °C, not full).",
                              "SS54 diodes soldered inline, under heatshrink.",
                              "DS18B20 probe taped between the cells."]):
        o.append(text(gx, gy + i * 21, line, 13 if i == 0 else 12, weight="bold" if i == 0 else "normal"))

    def R(k): x, y, w, h = box[k]; return (x + w, y + h / 2)
    def L(k): x, y, w, h = box[k]; return (x, y + h / 2)
    def T(k): x, y, w, h = box[k]; return (x + w / 2, y)
    def B(k): x, y, w, h = box[k]; return (x + w / 2, y + h)

    wire([R("sol"), L("isol")])
    wire([R("isol"), L("d1")])
    wire([R("jack"), L("d2")], "centre +")
    wire([R("d1"), (525, 115), (525, 165), L("q1")], "CHG_IN", lab_at=1)
    wire([R("d2"), (525, 215), (525, 165)], arrow=False)
    wire([R("q1"), L("chg")], "VIN+")
    wire([R("chg"), L("ibat")], "BAT+")
    wire([B("ibat"), T("pack")], "to P+")
    wire([L("pack"), R("fuse")], wgt=5)
    wire([L("fuse"), R("sw")], wgt=5)
    wire([L("sw"), R("main")], wgt=5)
    mx, my = B("main")
    wire([(mx, my), (mx, 375)], wgt=5, arrow=False)
    wire([(115, 375), (575, 375)], wgt=5, arrow=False)
    wire([(115, 375), T("tbl")], wgt=5)
    wire([(215, 375), (215, 495), R("tbr")], wgt=5)
    wire([(345, 375), T("xl")], wgt=5)
    wire([B("xl"), T("pca")], wgt=5)
    o.append(text(352, 515, "6.0 V", 11, fill="#333"))
    wire([(575, 375), T("mp")])
    wire([B("mp"), (575, 515), (515, 515), T("esp")])
    wire([(575, 515), (645, 515), T("pi")])
    o.append(text(582, 500, "5.1 V", 11, fill="#333"))
    wire([(770, 445), (745, 445), (745, 250), (635, 250), B("q1")], "coil: ESP32 GPIO4", "sig", 2, lab_at=2)
    o.append("</svg>")
    return "\n".join(o)


def main():
    IMG.mkdir(parents=True, exist_ok=True)
    for name, fn in (("wiring_car", car), ("wiring_remote", remote), ("wiring_power", power)):
        (IMG / f"{name}.svg").write_text(fn())
        print("wrote", IMG / f"{name}.svg")


if __name__ == "__main__":
    main()
