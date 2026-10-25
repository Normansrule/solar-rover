// Solar Rover handheld remote: ESP32 DevKit + 2 analog joysticks + 2 buttons,
// talking to the car over ESP-NOW (no router needed, ~100 m line of sight),
// and over an E220 LoRa module (~1 km+) when ESP-NOW stops getting through.
//
// Left stick: drive. Right stick: arm yaw/shoulder (hold B: wrist/elbow).
// A: gripper open/close. Left click: cycle mode manual -> auto -> agent.
// Right click: arm ready/park. Both clicks: STOP.
// Pairing: power the car (it accepts a new remote for 60 s after boot, or after
// the PAIR command), then power the remote. It scans channels until the car answers,
// and gets the LoRa key from the car while pairing.
// LED: solid = ESP-NOW, long on / short off = driving over LoRa, double blink =
// car battery low, fast blink = no link at all.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include "../../common/remote_packet.h"
#include "../../common/lora_link.h"

// ---- pins (see docs/wiring.md, "Handheld remote") ----
constexpr int PIN_LX = 34, PIN_LY = 35, PIN_RX = 32, PIN_RY = 33;   // ADC1 only (WiFi uses ADC2)
constexpr int PIN_LCLICK = 25, PIN_RCLICK = 26, PIN_A = 27, PIN_B = 14;
constexpr int PIN_LED = 2, PIN_VBAT = 39;   // "0-25 V voltage sensor" module S pin (5:1 divider)
constexpr int PIN_LORA_RX = 16, PIN_LORA_TX = 17;   // RX2 <- E220 TXD, TX2 -> E220 RXD
constexpr int PIN_LORA_MODE = 18;                    // E220 M0 + M1 together: high = setup/sleep, low = normal
constexpr int DEADZONE = 80;                // of 1000

uint8_t carMac[6]; bool paired = false; uint8_t channel = 1;
uint8_t seq = 0; uint32_t lastAck = 0; uint8_t carBatt = 0, carMode = 0;
int centre[4] = {2048, 2048, 2048, 2048};
const uint8_t BCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// ---- long-range link ----
HardwareSerial& radio = Serial2;
Preferences prefs;
uint8_t loraKey[16]; lora::Region region = lora::Region::US915;
bool haveKey = false, loraOk = false;
volatile bool keyArrived = false; LoraKeyPacket newKey;
lora::Failover failover;
lora::DutyBudget budget;
lora::Replay statusReplay;
uint32_t counter = 0, counterReserved = 0;          // counters never repeat, even across power cycles
uint32_t lastLoraStatus = 0; int carRssi = 0, myRssi = 0;
uint8_t lastLoraButtons = 0;
uint8_t rxBuf[48]; size_t rxLen = 0; uint32_t rxLastMs = 0;

uint32_t nextCounter() {
    if (++counter >= counterReserved) { counterReserved = counter + 1024; prefs.putULong("ctr", counterReserved); }
    return counter;
}

bool loraSetup() {
    uint8_t cfg[lora::CONFIG_LEN];
    lora::configFrame(cfg, region, loraKey);
    digitalWrite(PIN_LORA_MODE, HIGH);
    radio.updateBaudRate(9600);
    delay(80);
    while (radio.available()) radio.read();
    radio.write(cfg, sizeof cfg);
    uint8_t reply[lora::CONFIG_LEN]; size_t n = 0;
    uint32_t t0 = millis();
    while (n < sizeof reply && millis() - t0 < 300) if (radio.available()) reply[n++] = radio.read();
    digitalWrite(PIN_LORA_MODE, LOW);
    delay(80);
    radio.updateBaudRate(lora::UART_BAUD);
    budget.set(lora::profile(region).duty);
    bool ok = lora::configAccepted(reply, n, cfg);
    Serial.printf("LoRa E220: %s, %s ch %d\n", ok ? "ok" : "NOT FOUND", lora::profile(region).name, lora::profile(region).channel);
    return ok;
}

void loraSend(lora::Frame& f, uint8_t type) {
    size_t n = lora::seal(f, type, nextCounter(), loraKey);
    if (!budget.trySpend(millis(), lora::airtimeUs(n))) return;
    radio.write((const uint8_t*)&f, n);
}

void loraPoll() {
    uint32_t now = millis();
    while (radio.available()) { uint8_t b = radio.read(); if (rxLen < sizeof rxBuf) rxBuf[rxLen++] = b; rxLastMs = now; }
    if (!rxLen || now - rxLastMs < lora::RX_GAP_MS) return;
    lora::Frame f; int dbm = 0;
    if (lora::open(rxBuf, rxLen, loraKey, f, dbm) == lora::F_STATUS && statusReplay.accept(f.h.counter)) {
        lastLoraStatus = now; myRssi = dbm; carRssi = f.s.rssi;
        carBatt = f.s.batt_pct; carMode = f.s.mode;
        Serial.printf("LR car %u%% mode %u speed %d mm/s  rssi car %d / remote %d dBm\n",
                      f.s.batt_pct, f.s.mode, f.s.speed_mm_s, carRssi, myRssi);
    }
    rxLen = 0;
}

void onSent(const uint8_t* mac, esp_now_send_status_t st) {
    if (memcmp(mac, BCAST, 6) != 0) failover.result(st == ESP_NOW_SEND_SUCCESS, millis());
}

void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len == sizeof(LoraKeyPacket)) {
        LoraKeyPacket k; memcpy(&k, data, sizeof k);
        if (k.magic == RC_MAGIC && k.type == PKT_LORA_KEY) { newKey = k; keyArrived = true; }
        return;
    }
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
    esp_now_register_send_cb(onSent);
    addPeer(BCAST);

    prefs.begin("remote", false);
    counter = counterReserved = prefs.getULong("ctr", 0);
    haveKey = prefs.getBytes("lkey", loraKey, 16) == 16;
    region = (lora::Region)prefs.getUChar("lreg", 0);
    pinMode(PIN_LORA_MODE, OUTPUT);
    radio.begin(9600, SERIAL_8N1, PIN_LORA_RX, PIN_LORA_TX);
    if (haveKey) loraOk = loraSetup();
    else Serial.println("LoRa: no key yet, pair with the car first");
}

// Over LoRa: 10 Hz control while ESP-NOW is down (EU: buttons only, sent when
// they change and every 5 s), otherwise a 1 Hz ping so the car answers with its status.
void loraTick(uint32_t now, const RemotePacket& p, bool longRange) {
    static uint32_t lastTx = 0;
    if (!loraOk) return;
    loraPoll();
    lora::Profile pr = lora::profile(region);
    lora::Frame f{};
    if (longRange && pr.drive_ok) {
        if (now - lastTx < lora::CONTROL_MS) return;
        f.c.lx = lora::toByte(p.lx); f.c.ly = lora::toByte(p.ly); f.c.rx = lora::toByte(p.rx); f.c.ry = lora::toByte(p.ry);
        f.c.buttons = p.buttons; f.c.batt_pct = p.batt_pct;
        lastTx = now; loraSend(f, lora::F_CONTROL);
    } else if (longRange) {
        if (p.buttons == lastLoraButtons && now - lastTx < 5000) return;
        lastLoraButtons = p.buttons;
        f.c.buttons = p.buttons; f.c.batt_pct = p.batt_pct;
        lastTx = now; loraSend(f, lora::F_CONTROL);
    } else if (now - lastTx >= lora::PING_MS) {
        f.p.batt_pct = p.batt_pct;
        lastTx = now; loraSend(f, lora::F_PING);
    }
}

RemotePacket readInputs() {
    RemotePacket p;
    p.magic = RC_MAGIC; p.type = PKT_CONTROL; p.seq = seq++;
    p.lx = axis(PIN_LX, 0, false); p.ly = axis(PIN_LY, 1, true);   // stick up = forward
    p.rx = axis(PIN_RX, 2, false); p.ry = axis(PIN_RY, 3, true);
    p.buttons = (!digitalRead(PIN_LCLICK) ? BTN_LCLICK : 0) | (!digitalRead(PIN_RCLICK) ? BTN_RCLICK : 0) |
                (!digitalRead(PIN_A) ? BTN_A : 0) | (!digitalRead(PIN_B) ? BTN_B : 0);
    p.batt_pct = battPct();
    return p;
}

void loop() {
    static uint32_t lastHello = 0, lastSend = 0;
    static RemotePacket p = {};
    uint32_t now = millis();
    if (keyArrived) {                                      // pairing handed us the LoRa key
        keyArrived = false;
        memcpy(loraKey, newKey.key, 16); region = (lora::Region)newKey.region; haveKey = true;
        prefs.putBytes("lkey", loraKey, 16); prefs.putUChar("lreg", newKey.region);
        statusReplay.last = 0;
        loraOk = loraSetup();
    }
    if (paired && now - lastAck > 3000) paired = false;   // lost the car over ESP-NOW: rescan
    bool longRange = failover.longRange(now);

    if (now - lastSend >= 20) {                            // 50 Hz
        lastSend = now;
        p = readInputs();
        if (paired) {
            if (!esp_now_is_peer_exist(carMac)) addPeer(carMac);
            esp_now_send(carMac, (uint8_t*)&p, sizeof p);
            // keep-alive HELLO once a second so the car keeps answering with its status
            if (seq % 50 == 0) { RemotePacket h = p; h.type = PKT_HELLO; esp_now_send(carMac, (uint8_t*)&h, sizeof h); }
        } else if (now - lastHello > 120) {                // hop channels 1..13 sending HELLO
            lastHello = now;
            setChannel(channel % 13 + 1);
            RemotePacket h = {RC_MAGIC, PKT_HELLO, seq++, 0, 0, 0, 0, 0, battPct()};
            esp_now_send(BCAST, (uint8_t*)&h, sizeof h);
        }
    }
    loraTick(now, p, longRange);

    // LED: solid = ESP-NOW, long on / short off = LoRa, double blink = car battery low, fast blink = no link
    bool loraLive = loraOk && now - lastLoraStatus < 3000;
    bool low = carBatt < 30 && (paired || loraLive);
    bool on;
    if (low) on = (now / 150) % 6 < 3 && (now / 150) % 2 == 0;
    else if (paired && !longRange) on = true;
    else if (loraLive) on = (now % 1000) < 800;
    else on = (now / 100) % 2;
    digitalWrite(PIN_LED, on);
    delay(2);
}
