# Modules

**No custom circuit board and no perfboard.** Every electronic part is an off-the-shelf module. They connect with Dupont jumpers, screw terminals and Wago lever connectors, and mount on the two slotted printed trays with screws, zip ties or foam tape.

This is every bought electronic module: what it does in the car, the key specs, and what to check when you buy it. Prices and quantities are in [bom.csv](bom.csv), and pins are in [wiring.md](wiring.md).

## Brains

| Module | Job | Key specs / what to check |
|---|---|---|
| **ESP32-WROOM-32 DevKit, 38-pin** | Real-time controller. Runs motor PWM and encoders, the servos (through the PCA9685), sensors, battery and charger logic, and every radio link: WiFi web app, BLE, ESP-NOW to the handheld remote, and the E220 LoRa radio over a UART. | Get the *original* ESP32 (WROOM-32 / -32E/-32D), not S2/S3/C3. The pin map assumes the 38-pin board. It needs a CP2102 or CH340 USB chip for flashing. |
| **ESP32 38-pin screw-terminal expansion board** | The DevKit plugs into it. Every GPIO gets a screw terminal and a 3-pin S/V/G header, and the board has 3V3/5V/GND blocks and an I2C header. | About 77 × 54 mm. Set its jumper to 3.3 V. Feed it 5 V on the 5V terminal, not its DC jack (its linear regulator would run hot). **MEASURE** the hole spacing (`esp_exp_holes` in `config.scad`). |
| **Passive I2C hub** | Splits the I2C bus to the five I2C boards. | Any small board with several 4-pin (VCC, GND, SDA, SCL) 2.54 mm headers wired in parallel. Not a TCA9548A, which is a multiplexer and needs extra code. |
| **Raspberry Pi Zero 2 W** | Runs the RL agent (`agent/`): reads telemetry over UART at 20 Hz, runs the policy at 10 Hz, sends drive commands. Optional camera vision for the pick-up demo. | Use Raspberry Pi OS Lite **64-bit**. Draws about 0.6–1.5 W. Optional: run the agent on a PC over WiFi instead (`--url ws://solar-rover.local/ws`). |
| **OV5647 Pi camera (Zero version)** | Vision for the pick-up demo. | Needs the 22-pin "Zero" ribbon. Mounts on `camera_mount.stl`, hole pattern 21 × 12.5 mm, M2. |

## Drive

| Module | Job | Key specs |
|---|---|---|
| **TT gear motor 1:48, dual shaft** ×4 | One per wheel. | 3–6 V. About 200 rpm at 6 V (≈ 0.68 m/s with 65 mm wheels). Stall ≈ 1 A. The firmware caps PWM at 80% so the motors see about 6 V from the 7.4–8.4 V pack. **MEASURE** body size and hole positions (`tt_*` in `config.scad`). |
| **TB6612FNG breakout** ×2 | One per side. Each drives the front and rear motor of its side. | 1.2 A continuous / 3.2 A peak per channel, VM up to 15 V, logic 2.7–5.5 V (run at 3.3 V). Get the version with loose header pins and solder them pins-up; it snaps into `module_cradle_tb6612`. |
| **LM393 slot speed sensor** ×4 | Counts the 20 slots of the printed encoder disc: 40 edges per wheel turn ≈ 5.1 mm of travel per edge. | Fork slot ≈ 5 mm. Power at 3.3 V. |

## Arm

| Module | Job | Key specs |
|---|---|---|
| **MG996R** ×3 | Base yaw, shoulder, elbow. | ≈ 9–11 kg·cm at 4.8–6 V; stall ≈ 2.5 A each. The firmware moves one of these at a time so the 5 A rail isn't overloaded. Get the version with the **round metal horn**; the horn's 4 screw holes sit on a 15 mm circle (`mg996_horn_hole_r`; MEASURE). |
| **MG90S** ×2 | Wrist pitch, gripper. | ≈ 2 kg·cm. Ships with M2 self-tapping screws. |
| **PCA9685 16-channel** | Generates the 50 Hz servo pulses over I2C. | Address 0x40. V+ terminal feeds the servo power rail. Clone boards vary in hole spacing (`pca_holes` in `config.scad`). |

**Torque check** (estimated, at full horizontal reach with a 50 g load):

| Joint | Needed | MG996R gives | Margin |
|---|---|---|---|
| Shoulder | ≈ 3.3 kg·cm | ≈ 9 kg·cm | ≈ 2.7× |
| Elbow | ≈ 1.5 kg·cm | ≈ 9 kg·cm | ≈ 6× |

**Reach:** the shoulder axis is 148 mm above the ground. With the gripper pointing straight down, the fingertips reach the ground up to **17.5 cm from the turret axis, about 10 cm ahead of the nose** (`agent/solar_rover_agent/arm_ik.py`, checked against the 3D model by `tools/fit_check.py`).

## Sensors

| Module | Job | Key specs |
|---|---|---|
| **MPU-6050 (GY-521)** | Yaw rate for odometry and the policy; tilt. | 0x68, ±250 °/s, ±2 g, 100 Hz. Keep the car still for 1 s after power-on: the firmware measures gyro bias then. |
| **VL53L0X** | Forward distance (obstacles, approaching objects). | 0x29, up to ≈ 2 m indoors, 20 Hz. Sits behind the Ø9 mm window in the nose. |
| **INA219** ×2 | "solar": panel voltage and power, which is also the agent's light sensor. "batt": pack voltage and charge current. | 0.1 Ω shunt, 3.2 A max, 26 V max bus. Addresses set by solder pads (0x44 solar, 0x41 batt). |
| **DS18B20 waterproof probe + adapter module** | Pack temperature for the charge cut-off. | Get the probe with the little screw-terminal adapter board, which has the 4.7 kΩ pull-up. Power at 3.3 V. Charging is blocked outside 0–45 °C, with 3 °C hysteresis. A missing or unplugged probe counts as a fault and also blocks charging. |

## Power

| Module | Job | Key specs / settings |
|---|---|---|
| **4× 18650, 2S2P** | Main battery: 7.4 V nominal (6.6–8.4 V), ≈ 6 Ah, ≈ 44 Wh. | Use matched, genuine cells (Samsung 30Q, Molicel P28A, LG MJ1…). Never mix old and new cells. |
| **2S BMS (10–20 A, balance)** | Over-charge, over-discharge and short protection; balances the two cell groups. | Check it has a BM (middle) tap. |
| **CN3795 MPPT charger** | Charges the pack from the solar panels **or** the 12 V adapter (both diode-ORed into its input). | 6.6–30 V in, CC/CV up to 4 A. **Set 8.40 V and ≤ 1.5 A** before connecting the battery (wiring.md section 6). It has no temperature input or enable pin, so the ESP32 switches its input with the relay module. |
| **2× 6 V 2 W panels in series** | 12 V / ≈ 4 W peak; ≈ 12 Wh on a sunny day ≈ 1 hour of driving. | MEASURE the panel and set `panel` in `config.scad`; the roof trays are generated from it. |
| **1-channel 5 V relay module** | Charger input switch. | Get one with an optocoupler and an H/L trigger jumper; set it to H. GPIO4 HIGH = relay on = charging allowed. The coil draws about 70 mA, so the firmware only turns it on when there's sun or a wall adapter. |
| **2× SS54 Schottky diode** | Lets the panels and the wall adapter share the charger without feeding each other. | The only loose parts. Solder each one straight onto its wires and cover it with heatshrink. |
| **Wago 221 lever connectors** | The power rails: MAIN+, GND, 5V, CHG_IN, PACK+ (wiring.md section 5). | 221-415 (5-way) and 221-413 (3-way). Take 0.14–4 mm² wire, so 24 AWG up to 12 AWG. |
| **XL4015 buck (5 A)** | 6.0 V servo rail. | Add 1000 µF at the PCA9685. |
| **MP1584 buck (3 A)** | 5.1 V for the ESP32 and the Pi. | |
| **Fuse 7.5 A + rocker switch** | Main power. | The charger joins the pack *before* the switch. |

### Energy budget (estimated)

| State | Draw | Runtime on a full 44 Wh pack |
|---|---|---|
| Driving + agent + occasional arm moves | ≈ 13 W | ≈ 3 h |
| Parked, radios on, Pi running | ≈ 2.5 W | ≈ 17 h |
| Battery critical: Pi halted, radios off, ESP32 in 5 s light-sleep naps | ≈ 0.2–0.3 W | days (solar refills it) |
| Solar input, sunny day | ≈ 12 Wh/day | |
| Wall charging at 1.5 A | ≈ 13 W | full in ≈ 4 h |

## Long-range radio (car and remote)

**Ebyte E220-900T22D**, one in the car and one in the remote, about $8 each. A LoRa module (Semtech LLCC68 chip) with its own microcontroller, so the ESP32 just sends and receives bytes over a UART: it needs 2 data pins plus 1 for the mode, and no radio library. 22 dBm, 850–930 MHz, 2.54 mm pin header (no soldering), SMA antenna jack, 21 × 36 mm. Powered from the DevKit's 3V3 (about 110 mA while sending, 10 mA listening).

- **Why this one:** ESP-NOW reaches about 100 m. LoRa at 9.6 kbit/s reaches 1–3 km in the open with the stock antennas (*est.*, the vendor says 5 km at its slowest rate), passes through a wall or two, and is still fast enough for 10 control frames a second. An SPI LoRa board such as the RFM95 needs 5–6 pins and soldering; the car's ESP32 had only 3 pins left, two of them boot-strapping pins. 2.4 GHz long-range options (ESP32 "LR" WiFi mode, nRF24 + PA) reach a few hundred metres at best.
- **Get the 900 MHz version** (E220-**900**T22D) for the US (915 MHz) and Europe (868 MHz). The 400 MHz version needs different settings and isn't supported by the firmware.
- **Antennas:** a 3 dBi SMA swivel ("rubber duck") for the band, turned to stand vertical on both ends. The remote's goes on an SMA bulkhead pigtail through its case wall.

## Handheld remote

ESP32 DevKit, 2× KY-023 joysticks (powered at 3.3 V), 2 buttons, one 18650 cell with a TP4056 USB-C charger, an MT3608 boost set to 5.0 V, and a "0–25 V voltage sensor" module for the battery level. It talks ESP-NOW straight to the car, with no router needed, and has the same E220 LoRa radio as the car for long range (above).

**Pairing:** the car accepts a new remote during the first 60 s after power-on (or 60 s after the `PAIR` command / the app's "Pair a remote" button). It then remembers that remote's MAC and ignores all others.
