# Wiring, pin by pin

**No custom circuit board.** Everything is an off-the-shelf module joined with Dupont jumpers, screw terminals and Wago 221 lever connectors. The only loose parts are two Schottky diodes and a few capacitors, each soldered straight onto a wire or clamped in a terminal.

Every pin here matches `firmware/car/src/config.h` (car) and `firmware/remote/src/main.cpp` (remote). If you change a pin, change it in both places.

**Wire colours used below:** red = + supply, black = GND, yellow = signal, blue = I2C SDA, green = I2C SCL.

**Wire gauge:** use 18 AWG for battery, fuse, switch, motor-driver VM and the 6 V servo rail. Use 24–26 AWG for everything else.

> ⚠️ **Before you connect the battery**
> - Set both buck converters and the charger with a multimeter first (section 6). Their factory settings can be anything, and a wrong 5 V rail kills the Pi and ESP32.
> - Insert the fuse last.
> - Never short the battery leads. 18650 cells can deliver tens of amps.

## Diagrams

These are drawn from the firmware pin maps by `tools/wiring_diagram.py`, so they always match the code. The tables below have the same connections with more detail.

![ESP32 wiring on the car](img/wiring_car.png)

![Power and charging board](img/wiring_power.png)

![Handheld remote wiring](img/wiring_remote.png)

## 1. Overview

```
 4x 18650 (2S2P) ── 2S BMS ── XT30 ──┬── 7.5 A fuse ── main switch ── MAIN+ (7.4-8.4 V)
                                     │                                  ├── TB6612 left  VM ── FL + RL motors
  CN3795 BAT+ ── INA219 "batt" ──────┘                                  ├── TB6612 right VM ── FR + RR motors
  (charger output joins the pack                                        ├── XL4015 buck 6.0 V ── PCA9685 V+ ── 5 servos
   before the fuse and switch)                                          └── MP1584 buck 5.1 V ── ESP32 VIN, Pi 5V
                                                                                                  ESP32 3V3 ── logic boards
  Solar 2x 6 V in series ── INA219 "solar" ── SS54 ──┐
  12 V wall jack ───────────────────────────── SS54 ─┴── relay NO contact ── CN3795 VIN
                                                          (ESP32 GPIO4 turns the relay on)
```

Charging only happens while the main switch is **on**. That's deliberate: the ESP32 must be awake to watch the pack temperature and switch the charger off when it's too hot. With the switch off, the relay is off and nothing charges.

## 2. ESP32 DevKit (38-pin WROOM-32) on the car

Plug the DevKit into the **38-pin screw-terminal expansion board**. Every GPIO then has a screw terminal and a 3-pin S/V/G header, so each module connects with a Dupont jumper or a screwed wire. Set the board's power jumper to **3.3 V**. Feed it 5 V on its 5V/GND terminal, not on its DC jack.

| ESP32 pin | Goes to | Notes |
|---|---|---|
| 5V terminal on the expansion board | 5.1 V rail (MP1584 OUT+) | Power in. Don't also plug in USB with the battery on unless the DevKit has a VIN diode (most do). |
| GND | GND bus | Use at least two GND wires. |
| 3V3 | 3.3 V pins on the expansion board and the I2C hub | Feeds PCA9685 VCC, TB6612 VCC ×2, MPU-6050, VL53L0X, INA219 ×2, LM393 ×4 and the DS18B20 (about 60 mA total). |
| GPIO21 | I2C hub SDA | PCA9685, MPU-6050, VL53L0X and INA219 ×2 each plug into one hub port |
| GPIO22 | I2C hub SCL | same |
| GPIO16 (RX2) | Pi header pin 8 (GPIO14 TXD) | UART 115200 8N1 |
| GPIO17 (TX2) | Pi header pin 10 (GPIO15 RXD) | |
| GPIO15 | Pi header pin 5 (GPIO3), plain jumper wire | Pulled low briefly to wake a halted Pi. The ESP32 only ever pulls it low or lets go, so no resistor is needed. |
| GPIO26 | Left TB6612 AIN1 **and** BIN1 | Left-side direction (both left motors) |
| GPIO27 | Left TB6612 AIN2 **and** BIN2 | |
| GPIO25 | Left TB6612 PWMA | Front-left speed |
| GPIO33 | Left TB6612 PWMB | Rear-left speed |
| GPIO19 | Right TB6612 AIN1 **and** BIN1 | Right-side direction |
| GPIO23 | Right TB6612 AIN2 **and** BIN2 | |
| GPIO18 | Right TB6612 PWMA | Front-right speed |
| GPIO5 | Right TB6612 PWMB | Rear-right speed |
| GPIO13 | STBY on **both** TB6612s | The TB6612 has its own pull-down on STBY, so the motors stay off while the ESP32 boots |
| GPIO34 | Front-left LM393 DO | Input-only pin. The module has its own pull-up. |
| GPIO35 | Rear-left LM393 DO | |
| GPIO36 (VP) | Front-right LM393 DO | |
| GPIO39 (VN) | Rear-right LM393 DO | |
| GPIO32 | DS18B20 adapter module DAT | Adapter VCC → 3V3, GND → GND. The adapter has the 4.7 kΩ pull-up. Tape the probe tip between the cells. |
| GPIO4 | Relay module IN (charger switch, section 5) | HIGH = relay on = charger connected. Relay VCC → 5.1 V rail, GND → GND, trigger jumper on **H** (high level). |
| GPIO2 | On-board LED | Status: short flash = OK, solid = charging, slow blink = low battery, fast blink = critical |
| GPIO0, 12, 14 | unused | GPIO0 and GPIO12 are boot-strapping pins; leave them free. |

## 3. Motor drivers (2× TB6612FNG breakout)

| TB6612 pin | Left driver | Right driver |
|---|---|---|
| VM | MAIN+ (after fuse + switch) | MAIN+ |
| VCC | 3V3 | 3V3 |
| GND (both pins) | GND | GND |
| STBY | ESP32 GPIO13 | ESP32 GPIO13 |
| AIN1 + BIN1 (link them) | GPIO26 | GPIO19 |
| AIN2 + BIN2 (link them) | GPIO27 | GPIO23 |
| PWMA | GPIO25 | GPIO18 |
| PWMB | GPIO33 | GPIO5 |
| AO1 / AO2 | front-left motor red / black | front-right motor red / black |
| BO1 / BO2 | rear-left motor red / black | rear-right motor red / black |

- Optional: a 100 nF ceramic capacitor across each TT motor's terminals cuts electrical noise. Many TT motors come with one already fitted.
- **Polarity check:** after first boot, type `D 300 300` in the serial monitor. All four wheels should roll forward. If one wheel spins backward, swap that motor's two wires. The firmware already reverses the right side for you.

## 4. Sensors and servo board (I2C, 3.3 V)

The five I2C boards share one bus through a **passive I2C hub**: a small board whose ports are all wired in parallel, with no chip on it. Do not use a TCA9548A multiplexer board. Run one 4-wire female-female jumper from the expansion board's I2C header (3V3, GND, SDA, SCL) to the hub, then one 4-wire jumper from the hub to each module.

| Module | VCC | GND | SDA | SCL | Address | Other pins |
|---|---|---|---|---|---|---|
| PCA9685 16-ch servo board | 3V3 | GND | 21 | 22 | 0x40 | V+ screw terminal ← 6.0 V rail; OE → GND or leave open |
| MPU-6050 (GY-521) | 3V3 | GND | 21 | 22 | 0x68 | AD0 → GND; INT unused |
| VL53L0X (GY-VL53L0XV2) | 3V3 (VIN) | GND | 21 | 22 | 0x29 | XSHUT and GPIO1 unused |
| INA219 "batt" | 3V3 | GND | 21 | 22 | **0x41**: bridge the A0 pads | VIN+ ← CN3795 BAT+; VIN− → pack + (BMS P+) |
| INA219 "solar" | 3V3 | GND | 21 | 22 | **0x44**: bridge the A1 pads | VIN+ ← solar +; VIN− → SS54 diode D1 anode |

The battery INA219 sits in the **charge** line only (≤1.5 A), so its 0.1 Ω shunt never carries motor current. It still measures pack voltage, which the firmware uses for state of charge.

### Servos on the PCA9685

| Channel | Joint | Servo |
|---|---|---|
| 0 | Base yaw | MG996R |
| 1 | Shoulder | MG996R |
| 2 | Elbow | MG996R |
| 3 | Wrist pitch | MG90S |
| 4 | Gripper | MG90S |

Servo plug colours: brown = GND, red = V+, orange = signal. Clamp a 1000 µF 16 V capacitor's legs in the PCA9685 V+ screw terminal, together with the rail wires, with the stripe to −. Many PCA9685 boards also have a pad for it.

### Wheel encoders (4× LM393 slot sensor)

VCC → 3V3, GND → GND, DO → the GPIO in section 2. AO is unused. Power them at **3.3 V**, not 5 V, because GPIO34–39 are not 5 V tolerant.

## 5. Power and charging (modules on the power and charge trays, no board)

The PCA9685, both bucks, both TB6612s and the MAIN+, GND and 5V connectors sit on the **power tray**. The charging parts (CN3795, relay, both INA219s, and the CHG_IN and PACK+ connectors) sit on the **charge tray** over the battery, so the charging wires stay short.

The rails are **Wago 221 lever connectors**: strip 11 mm, lift the lever, push the wire in, close the lever. Use one connector per node:

| Connector | Node | What goes in |
|---|---|---|
| 221-415 (5-way) | **MAIN+** | switch output, TB6612 left VM, TB6612 right VM, XL4015 IN+, MP1584 IN+ |
| 221-415 (5-way) | **GND** | BMS P−, both TB6612 GND, XL4015 IN−, MP1584 IN−, CN3795 BAT− … add a second 221-415 linked to it if you run out |
| 221-415 (5-way) | **5V** | MP1584 OUT+, expansion board 5V, Pi pin 2, relay module VCC |
| 221-413 (3-way) | **CHG_IN** | D1 cathode, D2 cathode, relay COM |
| 221-413 (3-way) | **PACK+** | BMS P+ (via XT30), fuse input, INA219 "batt" VIN− |

### Battery and BMS (2S, with balance lead)

1. Wire the 4-slot holder as 2S2P: the cells in slots 1+2 are in parallel (group A), the cells in slots 3+4 are in parallel (group B), and group A is in series with group B.
2. BMS **B−** → group A −. BMS **BM** (middle) → the A+/B− junction. BMS **B+** → group B +.
3. BMS **P−** → GND. BMS **P+** → XT30 (battery side).

### Main power

XT30 + → PACK+ → inline blade fuse **7.5 A** → rocker switch → MAIN+. XT30 − → GND.

### Charger input and switch

- Panels: panel 1 − → panel 2 + (series). Panel 1 + = **solar +**, panel 2 − = **solar −**.
- Solar + → INA219 "solar" VIN+. Then INA219 "solar" VIN− → **D1** SS54 anode. D1 cathode (the striped end) → CHG_IN.
- DC jack centre pin (+) → **D2** SS54 anode. D2 cathode → CHG_IN.
- Solar − and the jack sleeve → GND.
- **Diodes:** solder each SS54 straight onto its wires, cover it with heatshrink, and clamp the cathode lead in the CHG_IN lever connector. These diodes stop the wall adapter feeding the panels, and stop the panels feeding the adapter.
- **Charger switch: 1-channel 5 V relay module.**

| Relay module pin | Connects to |
|---|---|
| VCC | 5V |
| GND | GND |
| IN | ESP32 GPIO4 |
| Trigger jumper | **H** (high level). If your module has no jumper, it must say "high level trigger". |
| COM (screw terminal) | CHG_IN |
| NO (screw terminal) | CN3795 VIN+ |
| NC | not used |

- The firmware only switches the relay on when charging is allowed (pack between 0 and 45 °C, not full) **and** there is something to charge from. That's either the panels above 9.5 V, or the wall adapter, which it checks for every 2 minutes. So the coil (about 70 mA) isn't on at night. You'll hear a click when it switches.
- CN3795 VIN− → GND.
- CN3795 BAT+ → INA219 "batt" VIN+. INA219 "batt" VIN− → PACK+. CN3795 BAT− → GND.

### Bucks

| Buck | IN+ / IN− | OUT+ goes to |
|---|---|---|
| XL4015 (5 A) | MAIN+ / GND | PCA9685 V+ terminal (servo rail) |
| MP1584 (3 A) | MAIN+ / GND | the 5V lever connector |

## 6. Set the modules BEFORE connecting the battery

Use a bench supply or the 12 V adapter with a multimeter.

1. **MP1584:** feed 8 V in, then turn the trimmer until OUT reads **5.10 V**.
2. **XL4015:** feed 8 V in, then turn until OUT reads **6.0 V**.
3. **CN3795 charge voltage:** with **no battery connected**, feed 12 V into VIN and turn the CV trimmer (if your board has one) until BAT reads **8.40 V**. Boards sold "for 2S / 8.4 V" are preset; check them anyway.
4. **CN3795 charge current:** I = 0.12 V / R<sub>CS</sub>. Read the current-sense resistor marking: R100 = 1.2 A and R080 = 1.5 A are good; R050 = 2.4 A is too much, so replace it.
5. **CN3795 MPPT:** if your board has an MPPT trimmer, put the panels in full sun, connect the battery, and adjust until the panel voltage under load is about **11 V**.

## 7. Raspberry Pi Zero 2 W

| Pi header pin | Signal | Connects to |
|---|---|---|
| 2 | 5V | 5.1 V rail |
| 6 | GND | GND bus |
| 8 | GPIO14 TXD | ESP32 GPIO16 (RX2) |
| 10 | GPIO15 RXD | ESP32 GPIO17 (TX2) |
| 5 | GPIO3 | ESP32 GPIO15 (wake), plain wire |
| CSI connector | camera | Pi Camera (OV5647, "Zero" 22-pin ribbon) |

Solder wires straight to the Pi pads, or use a right-angle header. A straight header plus Dupont plugs is too tall for the space under the roof.

## 8. Handheld remote

| Part | Pin | ESP32 DevKit |
|---|---|---|
| Left joystick (KY-023) | +5V pin → **3V3** (not 5 V) | 3V3 |
| | GND | GND |
| | VRx | GPIO34 |
| | VRy | GPIO35 |
| | SW | GPIO25 (internal pull-up) |
| Right joystick | +5V pin → 3V3, GND | |
| | VRx | GPIO32 |
| | VRy | GPIO33 |
| | SW | GPIO26 |
| Button A (gripper) | one leg GPIO27, other GND | |
| Button B (arm shift) | one leg GPIO14, other GND | |
| Battery sense | "0–25 V voltage sensor" module: its VCC/+ input ← TP4056 B+, GND ← B−, S → GPIO39, − → GND | The module is a 5:1 divider; leave its + pin unconnected |

**Power chain:** 18650 → TP4056 USB-C board (with protection) B+/B−. Then TP4056 OUT+ → slide switch → MT3608 boost IN+ (set to **5.0 V** before connecting) → ESP32 VIN. TP4056 OUT− → MT3608 IN− → ESP32 GND.
