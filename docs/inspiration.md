# Design Inspiration: Red Alert's Wheeled Repair Vehicle

## Which vehicle this is

The best match for "the 4-wheel repair car from Red Alert" is the **Allied IFV (Infantry Fighting Vehicle) from Command & Conquer: Red Alert 2 / Yuri's Revenge** when it carries an Engineer. Put an Engineer inside and the IFV's missile turret turns into a **repair crane** that fixes friendly vehicles in the field. It is cheap ($600), quick, and built on a wheeled armored-car body.

Other candidates I checked:
- **Red Alert 1** has no repair vehicle. Its Allied vehicles are tanks, trucks, the Ranger jeep and the tracked APC, and field repair comes from the Field Mechanic infantry (Aftermath). [cncnz RA1 Allied units](https://cncnz.com/games/red-alert/allied-units/)
- **Tiberian Sun's Mobile Repair Vehicle** (Nod, not a Red Alert game) is the other well-known C&C repair vehicle. It also has "an extendable arm which houses all of the tools needed to repair a vehicle", which fits our arm idea just as well. [C&C Wiki: Mobile repair vehicle](https://cnc.fandom.com/wiki/Mobile_repair_vehicle)

Source for the IFV repair crane: [cncnz RA2 Allied units](https://cncnz.com/games/red-alert-2/allied-units/). I'm describing the IFV's visual details below (low wedge body, four big wheels, turret on a central ring) from memory of the game art, so check them against screenshots before treating them as exact.

## What we borrow from it

| Red Alert feature | What it becomes on our robot car |
|---|---|
| Low, wide, wedge-nosed armored body on 4 large wheels | 4WD chassis with one N20 or TT gear motor per wheel and wide 3D-printed wheels with TPU tires. A low centre of gravity keeps it stable when the arm reaches out. |
| Swappable turret on a central ring | The arm mounts on a **turret ring with a standard bolt pattern** (for example 4 × M3 on a 60 mm circle). The arm, a camera mast or a gripper module can all drop onto the same ring, like the IFV swapping turrets. |
| Repair crane boom | A 4-DOF arm: base yaw on the turret ring, shoulder, elbow, and a gripper, driven by MG90S or MG996R servos. Folded, it rests along the hull like the crane in transport position. |
| Armored panels over the chassis | Snap-on or screwed **body shell panels** over a flat printed deck. The deck carries the electronics and the shell is purely cosmetic, so it can be reprinted or restyled without touching the wiring. |
| Allied blue-grey with hazard markings | Print the body in grey or blue PLA/PETG and the arm in yellow, with optional hazard-stripe decals on the crane. This keeps the "service vehicle" look. |
| Autonomous "guard mode" auto-repair | The onboard **RL agent** is the equivalent of guard mode: it drives itself, finds a target and uses the arm without a human. App or remote control is the manual-order mode. |

## How it maps onto the project goals

- **4WD driven by an RL agent:** Wheeled skid-steer is simple to model, so it suits training in simulation (for example a MuJoCo or Isaac model of the printed chassis) before running on the real car. Wheel encoders and an IMU give the agent good state data.
- **Robotic arm on top:** The turret ring sits over the centre of the wheelbase so the arm's reach stays inside the wheel footprint at rest. Battery weight goes at the rear as a counterweight.
- **All parts 3D printable:** Chassis deck, wheel hubs, tires (TPU), turret ring, arm links, gripper fingers and body shell are all printed. Only motors, servos, electronics, bearings and screws are bought.
- **Reasonable BOM:** The IFV look comes from the printed shell, not from expensive parts. A rough budget is ESP32 (BT + WiFi) for control, an optional Raspberry Pi or similar for the agent, 4 gear motors with 2 dual H-bridge drivers, 4 to 5 servos, a 2S Li-ion pack with a solar charger and panels, and an M2/M3 screw kit. The priced list comes to about $200.
- **Easy assembly with M2 or larger screws:** Use M3 for structural joints (deck, turret ring, arm base) and M2 or M2.5 only for servo horns and small brackets. Heat-set inserts or captive nut pockets mean no screw goes straight into plastic that gets taken apart often. Keep the number of screw sizes to two or three.
- **Bluetooth / WiFi phone app, remote, or onboard agent:** The ESP32 handles BT and WiFi links to the phone app and remote. The agent's computer sends the same drive and arm commands over serial, and a simple mode switch decides who is in control (manual, agent, or agent with manual override).

## Power: rechargeable battery + solar

Like a field unit that never has to return to base, the car carries a rechargeable 2S2P 18650 pack (7.4 V, about 44 Wh) and a solar roof: two 6 V 2 W panels in series behind the turret. The arm folds forward over the nose so it never shades them. One MPPT charger takes either the panels or a 12 V wall adapter, with a temperature cutoff so the pack doesn't charge while it's hot in the sun. When the battery runs low, the agent's job becomes finding sun and parking.

## Decisions made in the full design

- Scale: TT motors, 300 × 200 mm body (deck printed in two halves for a 220 mm bed).
- Agent: onboard Raspberry Pi Zero 2 W, with the option to run it on a PC over WiFi instead.

Full details are in [robot-car-design.md](robot-car-design.md) and the priced parts list in [bom.csv](bom.csv).
