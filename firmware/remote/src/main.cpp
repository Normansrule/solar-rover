// Solar Rover handheld remote: ESP32 DevKit + 2 analog joysticks + 2 buttons,
// talking to the car over ESP-NOW (no router needed, ~100 m line of sight).
//
// Left stick: drive. Right stick: arm yaw/shoulder (hold B: wrist/elbow).
// A: gripper open/close. Left click: cycle mode manual -> auto -> agent.
// Right click: arm ready/park. Both clicks: STOP.
// Pairing: power the car (it accepts a new remote for 60 s after boot, or after
// the PAIR command), then power the remote. It scans channels until the car answers.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "../../common/remote_packet.h"

// ---- pins (see docs/wiring.md, "Handheld remote") ----
constexpr int PIN_LX = 34, PIN_LY = 35, PIN_RX = 32, PIN_RY = 33;   // ADC1 only (WiFi uses ADC2)
constexpr int PIN_LCLICK = 25, PIN_RCLICK = 26, PIN_A = 27, PIN_B = 14;
constexpr int PIN_LED = 2, PIN_VBAT = 39;   // "0-25 V voltage sensor" module S pin (5:1 divider)
constexpr int DEADZONE = 80;                // of 1000

uint8_t carMac[6]; bool paired = false; uint8_t channel = 1;
uint8_t seq = 0; uint32_t lastAck = 0; uint8_t carBatt = 0, carMode = 0;
int centre[4] = {2048, 2048, 2048, 2048};
const uint8_t BCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(CarAck)) return;
    CarAck a; memcpy(&a, data, sizeof a);
    if (a.magic != RC_MAGIC || a.type != PKT_ACK) return;
    memcpy(carMac, mac, 6); paired = true; lastAck = millis();
    carBatt = a.batt_pct; carMode = a.mode;
}

void addPeer(const uint8_t* mac) {
    if (esp_now_is_peer_exist(mac)) esp_now_del_peer(mac);
    esp_now_peer_info_t p = {}; memcpy(p.peer_addr, mac, 6); p.channel = 0; p.encrypt = false;
    esp_now_add_peer(&p);
}

void setChannel(uint8_t ch) {
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    channel = ch;
}

int axis(int pin, int i, bool invert) {
    int raw = 0; for (int k = 0; k < 4; k++) raw += analogRead(pin); raw /= 4;
    int v = (raw - centre[i]) * 1000 / 2048;
    if (abs(v) < DEADZONE) return 0;
    v = v > 0 ? (v - DEADZONE) * 1000 / (1000 - DEADZONE) : (v + DEADZONE) * 1000 / (1000 - DEADZONE);
    v = constrain(v, -1000, 1000);
    return invert ? -v : v;
}

uint8_t battPct() {
    float v = analogReadMilliVolts(PIN_VBAT) * 5 / 1000.0f;
    return (uint8_t)constrain((v - 3.3f) / (4.2f - 3.3f) * 100, 0, 100);
}

void setup() {
    Serial.begin(115200);
    for (int p : {PIN_LCLICK, PIN_RCLICK, PIN_A, PIN_B}) pinMode(p, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);
    analogSetAttenuation(ADC_11db);
    // calibrate stick centres: leave the sticks alone while powering on
    int pins[4] = {PIN_LX, PIN_LY, PIN_RX, PIN_RY};
    for (int i = 0; i < 4; i++) { long s = 0; for (int k = 0; k < 32; k++) s += analogRead(pins[i]); centre[i] = s / 32; }
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_now_init();
    esp_now_register_recv_cb(onRecv);
    addPeer(BCAST);
}

void loop() {
    static uint32_t lastHello = 0, lastSend = 0;
    uint32_t now = millis();
    if (paired && now - lastAck > 3000) paired = false;   // lost the car: rescan

    if (!paired) {
        if (now - lastHello > 120) {                      // hop channels 1..13 sending HELLO
            lastHello = now;
            setChannel(channel % 13 + 1);
            RemotePacket h = {RC_MAGIC, PKT_HELLO, seq++, 0, 0, 0, 0, 0, battPct()};
            esp_now_send(BCAST, (uint8_t*)&h, sizeof h);
        }
        digitalWrite(PIN_LED, (now / 100) % 2);
        delay(5);
        return;
    }
    if (!esp_now_is_peer_exist(carMac)) addPeer(carMac);

    if (now - lastSend >= 20) {                            // 50 Hz
        lastSend = now;
        RemotePacket p;
        p.magic = RC_MAGIC; p.type = PKT_CONTROL; p.seq = seq++;
        p.lx = axis(PIN_LX, 0, false); p.ly = axis(PIN_LY, 1, true);   // stick up = forward
        p.rx = axis(PIN_RX, 2, false); p.ry = axis(PIN_RY, 3, true);
        p.buttons = (!digitalRead(PIN_LCLICK) ? BTN_LCLICK : 0) | (!digitalRead(PIN_RCLICK) ? BTN_RCLICK : 0) |
                    (!digitalRead(PIN_A) ? BTN_A : 0) | (!digitalRead(PIN_B) ? BTN_B : 0);
        p.batt_pct = battPct();
        esp_now_send(carMac, (uint8_t*)&p, sizeof p);
        // keep-alive HELLO once a second so the car keeps answering with its status
        if (seq % 50 == 0) { RemotePacket h = p; h.type = PKT_HELLO; esp_now_send(carMac, (uint8_t*)&h, sizeof h); }
    }
    // LED: solid = paired, double blink = car battery low
    bool low = carBatt < 30;
    digitalWrite(PIN_LED, low ? ((now / 150) % 6 < 3 && (now / 150) % 2 == 0) : HIGH);
    delay(2);
}
