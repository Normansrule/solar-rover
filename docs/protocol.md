# Control protocol

Every way of controlling the car ends up in the same parser (`firmware/car/src/protocol.cpp`):

| Link | Transport | Who uses it |
|---|---|---|
| USB serial | 115200 baud, one command per line | You, from `pio device monitor` |
| UART2 (GPIO16/17) | 115200 baud, lines | The Raspberry Pi agent |
| WiFi | WebSocket `ws://<car>/ws`, one or more lines per frame | The phone web app, the agent from a PC |
| Bluetooth LE | Nordic UART Service (`6E400001-…`), write to RX `6E400002-…`, notify on TX `6E400003-…` | Phone app (Chrome on Android), Bluefruit Connect, nRF Toolbox |
| ESP-NOW | Binary `RemotePacket` (`firmware/common/remote_packet.h`) | The handheld remote |
| LoRa (E220, UART1 on GPIO13/14) | Signed binary frames (`firmware/common/lora_link.h`) | The handheld remote, beyond ESP-NOW range (below) |

## WiFi

- The car always starts its own access point, **`SolarRover-XXXX`**, password **`solarrover123`** (change `AP_PASSWORD` in `config.h`). The app is then at **http://192.168.4.1**.
- If you save a home network (`WIFI <ssid> <password>`, or the app's WiFi box, then restart), it also joins that network. Open **http://solar-rover.local** (mDNS), or the IP that `INFO` prints.

## Commands

Values are integers unless noted. Speeds run from −1000 to 1000 (= −100 % to 100 %).

| Command | Meaning |
|---|---|
| `D <left> <right>` | Tank drive: left and right side speed |
| `V <throttle> <turn>` | Arcade drive; positive turn = turn left |
| `A <yaw> <shoulder> <elbow> <wrist> <grip>` | Arm joint targets in degrees; `*` keeps a joint as it is |
| `J <joint> <deg/s>` | Jog one joint at a speed (joint = `yaw`, `shoulder`, `elbow`, `wrist`, `grip` or 0–4) |
| `G <0..100>` | Gripper, 0 = closed, 100 = fully open |
| `P <park\|ready\|pick\|carry>` | Named arm pose |
| `M <manual\|auto\|agent>` | Control mode (below) |
| `S` | Stop driving and hold the arm |
| `H` | Heartbeat: "I'm still here", keeps the watchdog happy without moving |
| `CL <0\|1>` | Closed-loop wheel speed from the encoders on/off (default on) |
| `CAL <joint> <offset_deg> <dir>` | Servo trim and direction (1 or −1), saved in flash |
| `WIFI <ssid> <password>` / `WIFI AP` | Save a home network / go back to AP only (not accepted from the remote) |
| `PAIR` | Accept a new handheld remote for 60 s (it also gets the LoRa key) |
| `LORA` | Long-range link status: `LORA ok US915 rssi=-92 last=0s` |
| `LORA REGION <US\|EU>` | Radio region (frequency, power, duty cycle), saved in flash. Opens pairing for 60 s so the remote picks it up |
| `LORA REKEY` | New random LoRa key (if the remote was lost). Opens pairing for 60 s |
| `RESTART` | Reboot the ESP32 |
| `INFO` | Replies `solar-rover fw 0.1 mode=… ip=…` |

Errors come back as `ERR <reason>`, for example `ERR mode` when the current mode doesn't let that link drive.

Bluefruit Connect's **Control Pad** also works over BLE: the arrows drive, 1 toggles the gripper, 2 = ready pose, 3 = park pose, 4 = stop.

## Modes and safety

| Mode | Who may drive |
|---|---|
| `manual` | Remote, app (WiFi/BLE), USB. The agent is ignored. |
| `auto` (default) | Everyone. Any manual input takes over for 2 s, then the agent gets control back. |
| `agent` | Only the Pi agent. |

- **Watchdog:** if the link that is driving sends nothing for 300 ms, the motors stop (700 ms for the LoRa link, which sends at 10 Hz). Senders repeat their command (or `H`) at 10 Hz or more.
- `LORA` commands are not accepted from the remote, the LoRa link or the agent.
- `S` and `M` are accepted from every link in every mode.
- **Battery low (30 %):** the `low` flag goes to 1. The agent stops its task and looks for sun.
- **Battery critical (15 %):** motors stop, the arm parks, the Pi gets `SHUTDOWN` and powers off, radios turn off, and the ESP32 naps in light-sleep while the charger keeps running. When solar brings it back above 60 %, it wakes the Pi and restarts.

## Telemetry

The car sends one JSON line at 20 Hz on the UART and 10 Hz to WebSocket and BLE clients:

```json
{"t":123456,"mode":"auto","src":"agent","ovr":0,
 "bat_v":7.86,"bat_pct":72,"chg_ma":310,"sol_v":11.2,"sol_mw":3450,
 "temp_c":24.5,"chg":"solar","low":0,"crit":0,
 "enc":[1020,1018,995,1001],"spd":[210,205,198,202],
 "imu":[0.012,-0.004,0.998,0.4,-0.2,12.5],"tof":640,
 "arm":[0.0,45.0,60.0,-10.0,60]}
```

| Field | Meaning |
|---|---|
| `t` | ms since boot |
| `mode`, `src`, `ovr` | Current mode, which link drove last, 1 while a manual override is holding |
| `bat_v`, `bat_pct` | Pack voltage and estimated charge |
| `chg_ma`, `chg` | Charge current and charger state (`idle`, `solar`, `wall`, `full`, `off_temp` = too hot or cold, `fault` = temperature probe missing) |
| `sol_v`, `sol_mw` | Panel voltage and power: also the agent's light sensor |
| `temp_c` | Pack temperature (DS18B20 probe) |
| `enc`, `spd` | Encoder counts and wheel speed in mm/s: FL, RL, FR, RR |
| `imu` | ax, ay, az in g; gx, gy, gz in °/s |
| `tof` | Front distance in mm (−1 = nothing within 2 m) |
| `arm` | Joint angles in degrees: yaw, shoulder, elbow, wrist, grip % |

`lr` (at the end of every line) is the long-range link: `[module ok 0/1, RSSI of the last frame in dBm (0 = none yet), ms since that frame (999999 = never)]`, for example `"lr":[1,-92,40]`. `src` is `lora` while the remote drives over LoRa.

Lines that aren't JSON are replies to commands (`OK …`, `ERR …`, `SHUTDOWN`).

## Handheld remote mapping

| Control | Action |
|---|---|
| Left stick | Drive (up = forward, sideways = turn) |
| Right stick | Arm yaw (x) and shoulder (y) |
| Right stick + hold **B** | Wrist (x) and elbow (y) |
| **A** | Gripper open / close |
| Left stick click | Cycle mode: manual → auto → agent |
| Right stick click | Arm ready ↔ park |
| Both stick clicks | Stop |

## Long-range link (LoRa)

The car and the remote each have an **Ebyte E220-900T22D** LoRa module (modules.md). ESP-NOW stays the main remote link, because it's fast (50 Hz) and needs no setup. LoRa takes over when ESP-NOW stops getting through, at about 100 m, and reaches 1–3 km in the open (*est.*).

**Who decides:** the remote. ESP-NOW reports whether each packet was delivered. After 400 ms with no delivery, the remote drives over LoRa; after 10 deliveries in a row it goes back to ESP-NOW. While ESP-NOW is fine, it sends a LoRa ping once a second so you can see the signal strength before you need it.

**What the car does:**

- A LoRa control frame means exactly what the ESP-NOW packet means (same sticks and buttons), so the remote mapping below doesn't change.
- It ignores LoRa control frames while ESP-NOW packets are still arriving (within 250 ms), so a button press is never counted twice.
- Over LoRa, top speed is cut to **60 %** (`LORA_SPEED_SCALE`), and the watchdog allows **700 ms** of silence (7 lost frames) instead of 300 ms.
- At the switch-over the ESP-NOW watchdog stops the car for about half a second, then the LoRa frames take control again. That's on purpose: it never keeps rolling on a stale command.
- At critical battery the module goes to sleep with the other radios.

**Frames** (`firmware/common/lora_link.h`), little-endian, all signed:

| Frame | Direction | Bytes | Contents |
|---|---|---|---|
| Control | remote → car | 16 | sync `A7`, type 1, counter (u32), sticks lx ly rx ry (−100..100), buttons, remote battery %, tag |
| Ping | remote → car | 11 | sync, type 2, counter, remote battery %, tag |
| Status | car → remote | 16 | sync, type 3, counter, car battery %, mode, flags (low, critical, charging, LoRa driving), RSSI of the last frame the car heard, mean wheel speed (mm/s), tag |

- **Tag:** the first 4 bytes of SipHash-2-4 over the frame, with a 16-byte key the car makes from its hardware random number generator on first boot. The car hands the key to the remote over ESP-NOW only while pairing, so it isn't on the air every second. Frames with a wrong tag are dropped.
- **Counter:** goes up by one per frame and never repeats, even across power cycles (the remote reserves blocks of 1024 in flash). The car drops any frame whose counter isn't higher than the last one it accepted, so a recorded frame can't be played back. After `PAIR` the car resets this floor, because a re-flashed remote starts again from 0.
- **Half duplex:** only the remote talks first. The car answers every ping and every 5th control frame with a Status frame. At 9.6 kbit/s a control frame takes about 23 ms in the air and a status frame about 23 ms, so both fit in each 100 ms slot without colliding.
- **Module address:** the E220's own address and scrambling key come from the shared key too, so two rovers on the same channel don't hear each other.
- **Module setup:** the firmware writes the E220's registers at every boot (setup mode, M0 = M1 = high): 38400 baud, 9.6 kbit/s air rate, RSSI byte after each packet, transparent mode, channel and power from the region. No configuration tool or USB adapter is needed.

**Regions** (`LORA REGION`):

| Region | Frequency | Power | Airtime budget | Over LoRa |
|---|---|---|---|---|
| `US` (default) | 915.125 MHz | 22 dBm | 50 % (sanity cap) | Full driving at 10 Hz |
| `EU` | 868.125 MHz | 13 dBm | 1 % (the band's duty-cycle limit) | Buttons only: STOP, mode, arm ready/park, gripper. Sent when they change and every 5 s. The sticks don't work, because 1 % allows about one frame every 2 s |

The duty budget is a rolling credit (up to one minute's worth), so a few button presses at the start always get through. Check your own country's rules for the band and power before you transmit.

**Not built: cellular.** A 4G link would let you drive from anywhere with coverage, with video, but it needs a data SIM (a monthly cost), adds 50–150 ms of latency that varies, and depends on a server or VPN in between. If you want it later, the way that changes nothing on the car is to put the Raspberry Pi on a phone hotspot or a USB 4G stick (a "HiLink" stick shows up as a network card), join the Pi and your laptop to a Tailscale network, and add a small bridge on the Pi that passes protocol lines between a network socket and the UART (not written yet), plus the camera stream. It isn't worth it for a car that drives at walking pace and that you need to see to drive safely; LoRa covers that.
