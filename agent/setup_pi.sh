#!/usr/bin/env bash
# Set up the Raspberry Pi Zero 2 W (Raspberry Pi OS Lite, 64-bit) for the agent.
# Run on the Pi:  bash ~/solar-rover/agent/setup_pi.sh
set -euo pipefail
REPO="$HOME/solar-rover"
cd "$REPO/agent"

# 1. UART on GPIO14/15 for the ESP32, without the login console on it
sudo raspi-config nonint do_serial_hw 0      # enable the serial hardware
sudo raspi-config nonint do_serial_cons 1    # disable the serial console
sudo raspi-config nonint do_i2c 1            # keep Pi I2C off: GPIO3 is the wake line
# 2. Python environment
sudo apt-get update
sudo apt-get install -y python3-venv python3-numpy
python3 -m venv --system-site-packages .venv
.venv/bin/pip install -r requirements.txt
# 3. allow the agent to power the Pi off when the battery is critical
echo "$USER ALL=(root) NOPASSWD: /usr/bin/systemctl poweroff" | sudo tee /etc/sudoers.d/solar-rover-poweroff >/dev/null
# 4. start on boot
sed "s#/home/pi#$HOME#g; s#User=pi#User=$USER#" systemd/solar-rover-agent.service | sudo tee /etc/systemd/system/solar-rover-agent.service >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable solar-rover-agent
echo "Done. Reboot once (sudo reboot) so the UART change applies; the agent then starts by itself."
echo "Logs: journalctl -u solar-rover-agent -f"
