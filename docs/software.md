# Software: flash, set up, train, run

All commands are for Ubuntu (22.04 or 24.04) and start from an absolute path, so it doesn't matter which folder your terminal is in. They assume the repo is at `~/solar-rover` (see [github-setup.md](github-setup.md)).

## 1. Car firmware (ESP32)

Install PlatformIO once:

```bash
sudo apt update && sudo apt install -y python3-venv python3-pip git
python3 -m pip install --user pipx && python3 -m pipx ensurepath
~/.local/bin/pipx install platformio
sudo usermod -aG dialout "$USER"     # serial port access: log out and back in once after this
```

Plug the car's ESP32 in over USB (battery switch off is fine for flashing), then build, flash and open the serial monitor:

```bash
cd "$HOME/solar-rover/firmware/car"
~/.local/bin/pio run -t upload
~/.local/bin/pio device monitor
```

- The first build downloads the ESP32 toolchain and libraries (a few hundred MB).
- `scripts/embed_web.py` gzips `web/index.html` into `src/web_index.h` on every build, so edits to the app go in with the next flash.
- If the upload stalls at "Connecting…", hold the board's **BOOT** button until it starts.

**First checks in the serial monitor** (type and press Enter):

1. `INFO`: prints the firmware version, mode and IP.
2. Lift the car off the table, then `D 300 300` and keep sending it (the watchdog stops the wheels 300 ms after the last command). All four wheels should turn forward. Swap the wires of any motor that turns backward (wiring.md section 3).
3. `P ready`, then `P park`: the arm should move one joint at a time.
4. If a joint is off-centre or moves the wrong way, trim it: `CAL shoulder 5 1` (offset in degrees, direction 1 or −1). It's saved in flash.

## 2. Handheld remote

```bash
cd "$HOME/solar-rover/firmware/remote"
~/.local/bin/pio run -t upload
```

Pair it: switch the car on, then switch the remote on within 60 s. To pair a different remote later, send `PAIR` (or tap "Pair a remote" in the app) and switch that remote on within 60 s.

## 3. Phone app

1. On the phone, join WiFi **SolarRover-XXXX**, password `solarrover123`.
2. Open **http://192.168.4.1**. The app connects by itself.
3. To use your home WiFi instead, enter it in the app's WiFi box and tap Restart. The car then also appears at **http://solar-rover.local** (or the IP shown in the app) on your home network. Its own `SolarRover-XXXX` network stays on as a fallback.

**Bluetooth:** browsers only allow Bluetooth on an `https://` page, and the car serves its app over plain `http://`. So for Bluetooth, either:
- use the free **Adafruit Bluefruit Connect** app (Android and iPhone): connect to "SolarRover", then Controller → Control Pad; or
- open the same app from GitHub Pages in Chrome on Android or a PC. This needs a public repo: Settings → Pages → deploy from branch `main`. The page is then at `https://<you>.github.io/solar-rover/firmware/car/web/`. iPhone Safari has no Web Bluetooth; use Bluefruit Connect there.

## 4. Simulator and training (on your PC)

```bash
cd "$HOME/solar-rover/sim"
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/python -m pytest -q                      # 6 tests
.venv/bin/python -m solar_rover_sim.train_es --generations 400 --task mixed --out "$HOME/solar-rover/agent/models/drive"
```

- `train_es` uses only numpy and all CPU cores. 400 generations took about 8 minutes on 4 cores. It writes `drive.npz`, `drive.onnx` and `drive.json` (the eval scores).
- The included policy (`agent/models/drive.*`) reached the goal in 22 of 30 simulated runs, with 8 collisions. Sun-seeking uses a scripted search by default (`sun_mode: scripted` in `agent/config.yaml`) because it beat the learned policy in the sim, 23/40 against 8/40.
- **Optional PPO:** `.venv/bin/pip install stable-baselines3` (pulls in PyTorch), then `.venv/bin/python -m solar_rover_sim.train_ppo --steps 2000000 --out ppo_drive`. It writes `ppo_drive.zip` and `ppo_drive.onnx`; copy the `.onnx` to `agent/models/` and point `agent/config.yaml` at it. Tested with stable-baselines3 2.9 and PyTorch 2.14 (`sim/tests/test_ppo.py` checks the exported ONNX gives the same actions as SB3).
- **3-D model:** `.venv/bin/python -m solar_rover_sim.mujoco_view --view` opens the MuJoCo viewer of `models/solar_rover.xml`.

## 5. Raspberry Pi Zero 2 W (onboard agent)

1. Flash **Raspberry Pi OS Lite (64-bit)** with Raspberry Pi Imager. In its settings, set a username, your WiFi and enable SSH.
2. SSH in, then:

```bash
sudo apt update && sudo apt install -y git
git clone https://github.com/<your-user>/solar-rover.git "$HOME/solar-rover"
bash "$HOME/solar-rover/agent/setup_pi.sh"
sudo reboot
```

For a private repo, `git clone` asks for your GitHub username and a personal access token as the password (github.com → Settings → Developer settings → Tokens), or install `gh` on the Pi and run `gh auth login` first.

- `setup_pi.sh` enables the UART for the ESP32, makes a Python environment, lets the agent power the Pi off when the battery is critical, and installs a service that starts the agent on boot.
- Watch it: `journalctl -u solar-rover-agent -f`
- Test without moving anything: `cd "$HOME/solar-rover/agent" && sudo systemctl stop solar-rover-agent && .venv/bin/python -m solar_rover_agent.main --dry-run`

## 6. Run the agent from a PC instead (no Pi needed)

With your PC on the same WiFi as the car (or joined to `SolarRover-XXXX`):

```bash
cd "$HOME/solar-rover/agent"
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/python -m pytest -q                        # 10 tests
.venv/bin/python -m solar_rover_agent.main --url ws://solar-rover.local/ws
```

Use `ws://192.168.4.1/ws` when the PC is on the car's own network. In the app, set the mode to **auto** (you can take over any time) or **agent**.

## 7. Agent settings (`agent/config.yaml`)

| Key | Meaning |
|---|---|
| `task` | `navigate` (drive the waypoint loop) or `pick` (find an object with the camera, drive up, grab it) |
| `waypoints` | Metres from the start point, x forward, y left |
| `speed_scale` | Extra speed limit on top of the policy |
| `sun_mode` | `scripted` or `policy` for low-battery sun seeking |
| `policy` | `.npz` from `train_es` or any `.onnx` (needs `onnxruntime`) |

The `pick` task needs OpenCV and the camera: `sudo apt install -y python3-opencv python3-picamera2` on the Pi.

## 8. 3D files

The STLs in `hardware/stl/` are ready to print. To change a part, edit `hardware/scad/config.scad` (all the dimensions, including every **MEASURE** item) and re-export:

```bash
sudo apt install -y openscad
bash "$HOME/solar-rover/tools/export_stl.sh"            # rebuilds every STL
python3 "$HOME/solar-rover/tools/stl_check.py"           # checks each part fits a 220 mm bed
openscad "$HOME/solar-rover/hardware/scad/assembly.scad"   # whole car in the viewer
```

After changing a part, a module size or an arm pose, run the collision check. It exports every placed part from `assembly.scad`, including the arm in each pose from `firmware/car/src/arm.cpp` at three turret angles, and reports any two that overlap:

```bash
cd "$HOME/solar-rover"
sudo apt install -y openscad python3-venv
python3 -m venv "$HOME/solar-rover/.venv"                                      # once
"$HOME/solar-rover/.venv/bin/pip" install trimesh==5.1.1 manifold3d==3.5.4    # once
"$HOME/solar-rover/.venv/bin/python" "$HOME/solar-rover/tools/fit_check.py"      # "No collisions." or a list of clashes
"$HOME/solar-rover/.venv/bin/python" "$HOME/solar-rover/tools/fit_check.py" --paths   # also every move between poses
```

If you change a pin in `firmware/car/src/config.h` or the remote's `main.cpp`, redraw the wiring diagrams:

```bash
python3 "$HOME/solar-rover/tools/wiring_diagram.py"         # writes docs/img/wiring_*.svg
```

## 9. README animations and progress tracker

The README's GIFs are rendered from the 3D model. Redraw them after changing a part or an arm pose:

```bash
cd "$HOME/solar-rover"
sudo apt install -y openscad xvfb
"$HOME/solar-rover/.venv/bin/pip" install pillow                           # once
"$HOME/solar-rover/.venv/bin/python" "$HOME/solar-rover/tools/render_anim.py"  # build.gif, spin.gif, arm.gif
```

The progress tracker (bars, roadmap and checklist) comes from `docs/progress.yaml`. When you finish a task, set its `status` to `done`, mark the next one `next`, and run:

```bash
"$HOME/solar-rover/.venv/bin/pip" install pyyaml                           # once
"$HOME/solar-rover/.venv/bin/python" "$HOME/solar-rover/tools/progress.py"
```

CI fails with "out of date" if the README and pictures don't match `progress.yaml`.
