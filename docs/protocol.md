# Control protocol

Every way of controlling the car ends up in the same parser (`firmware/car/src/protocol.cpp`):

| Link | Transport | Who uses it |
|---|---|---|
| USB serial | 115200 baud, one command per line | You, from `pio device monitor` |
| UART2 (GPIO16/17) | 115200 baud, lines | The Raspberry Pi agent |
| WiFi | WebSocket `ws://<car>/ws`, one or more lines per frame | The phone web app, the agent from a PC |
| Bluetooth LE | Nordic UART Service (`6E400001-…`), write to RX `6E400002-…`, notify on TX `6E400003-…` | Phone app (Chrome on Android), Bluefruit Connect, nRF Toolbox |
| ESP-NOW | Binary `RemotePacket` (`firmware/common/remote_packet.h`) | The handheld remote |

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
| `PAIR` | Accept a new handheld remote for 60 s |
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

- **Watchdog:** if the link that is driving sends nothing for 300 ms, the motors stop. Senders repeat their command (or `H`) at 10 Hz or more.
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
