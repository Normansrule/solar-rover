#include "longrange.h"
#include "links.h"
#include "control.h"
#include "settings.h"
#include "../../common/lora_link.h"
#include "../../common/remote_packet.h"

namespace {
HardwareSerial& radio = Serial1;
uint8_t key[16];
lora::Region region = lora::Region::US915;
lora::Replay replay;
lora::DutyBudget budget;
uint32_t savedCounter = 0, txCounter = 0;
bool moduleOk = false, asleep = false;
int rssi = 0;
uint32_t lastGood = 0;
uint8_t rx[48]; size_t rxLen = 0; uint32_t rxLastMs = 0;
uint8_t controlsSinceStatus = 0;

bool configureModule() {
    uint8_t cfg[lora::CONFIG_LEN];
    lora::configFrame(cfg, region, key);
    digitalWrite(PIN_LORA_MODE, HIGH);           // M0 = M1 = 1: setup mode, always 9600 baud
    radio.updateBaudRate(9600);
    delay(80);
    while (radio.available()) radio.read();
    radio.write(cfg, sizeof cfg);
    uint8_t reply[lora::CONFIG_LEN]; size_t n = 0;
    uint32_t t0 = millis();
    while (n < sizeof reply && millis() - t0 < 300) if (radio.available()) reply[n++] = radio.read();
    bool good = lora::configAccepted(reply, n, cfg);
    digitalWrite(PIN_LORA_MODE, LOW);            // normal (transparent) mode
    delay(80);
    radio.updateBaudRate(lora::UART_BAUD);
    budget.set(lora::profile(region).duty);
    return good;
}

void sendStatus() {
    lora::Frame f{};
    f.s.batt_pct = (uint8_t)gPower.pct;
    f.s.mode = (uint8_t)control::mode();
    f.s.flags = (gPower.low ? lora::ST_LOW : 0) | (gPower.critical ? lora::ST_CRIT : 0) |
                (gPower.charger_on ? lora::ST_CHARGING : 0) | (control::driver() == Source::LORA ? lora::ST_LORA_DRIVES : 0);
    f.s.rssi = (int8_t)constrain(rssi, -128, 0);
    float v = 0; for (int i = 0; i < 4; i++) v += gDrive.speed_mm_s[i];
    f.s.speed_mm_s = (int16_t)constrain(v / 4, -32000.0f, 32000.0f);
    size_t n = lora::seal(f, lora::F_STATUS, ++txCounter, key);
    if (!budget.trySpend(millis(), lora::airtimeUs(n))) return;
    radio.write((const uint8_t*)&f, n);
}

void handlePacket(const uint8_t* buf, size_t n) {
    lora::Frame f; int dbm = 0;
    uint8_t type = lora::open(buf, n, key, f, dbm);
    if (!type || type == lora::F_STATUS || !replay.accept(f.h.counter)) return;
    rssi = dbm; lastGood = millis();
    if (f.h.counter > savedCounter + 256) { savedCounter = f.h.counter; settings::saveLoraCounter(savedCounter); }
    if (type == lora::F_PING) { sendStatus(); return; }
    // F_CONTROL: same meaning as the ESP-NOW packet; links decides whether to use it
    RemotePacket p{};
    p.magic = RC_MAGIC; p.type = PKT_CONTROL; p.seq = (uint8_t)f.h.counter;
    p.lx = lora::fromByte(f.c.lx); p.ly = lora::fromByte(f.c.ly);
    p.rx = lora::fromByte(f.c.rx); p.ry = lora::fromByte(f.c.ry);
    p.buttons = f.c.buttons; p.batt_pct = f.c.batt_pct;
    if (!lora::profile(region).drive_ok) p.lx = p.ly = p.rx = p.ry = 0;   // EU: buttons only
    links::remoteInput(p, Source::LORA);
    if (++controlsSinceStatus >= lora::STATUS_EVERY) { controlsSinceStatus = 0; sendStatus(); }
}
}  // namespace

namespace longrange {

void begin() {
    settings::loraKey(key);
    region = (lora::Region)settings::loraRegion();
    replay.last = savedCounter = settings::loraCounter();
    pinMode(PIN_LORA_MODE, OUTPUT);
    radio.begin(9600, SERIAL_8N1, PIN_LORA_RX, PIN_LORA_TX);
    moduleOk = configureModule();
    Serial.printf("LoRa E220: %s, %s ch %d\n", moduleOk ? "ok" : "NOT FOUND (check wiring, docs/wiring.md 2a)",
                  lora::profile(region).name, lora::profile(region).channel);
}

void poll() {
    if (!moduleOk || asleep) return;
    uint32_t now = millis();
    while (radio.available()) {
        uint8_t b = radio.read();
        if (rxLen < sizeof rx) rx[rxLen++] = b;
        rxLastMs = now;
    }
    if (rxLen && now - rxLastMs >= lora::RX_GAP_MS) { handlePacket(rx, rxLen); rxLen = 0; }
}

void sleep() {
    if (!moduleOk) return;
    asleep = true;
    digitalWrite(PIN_LORA_MODE, HIGH);          // mode 3 is also the module's sleep mode
}

bool ok() { return moduleOk; }
int lastRssi() { return rssi; }
uint32_t lastFrameMs() { return lastGood; }
const char* regionName() { return lora::profile(region).name; }

void fillKeyPacket(uint8_t* out, size_t& n) {
    // a remote that was re-flashed starts its counter again, so pairing resets the floor
    replay.last = savedCounter = 0; settings::saveLoraCounter(0);
    LoraKeyPacket k{RC_MAGIC, PKT_LORA_KEY, (uint8_t)region, {}};
    memcpy(k.key, key, 16);
    memcpy(out, &k, sizeof k); n = sizeof k;
}

void setRegion(uint8_t r) {
    region = (lora::Region)r;
    settings::setLoraRegion(r);
    if (!asleep) moduleOk = configureModule();
}

void rekey() {
    settings::newLoraKey();
    settings::loraKey(key);
    replay.last = savedCounter = 0;
    if (!asleep) moduleOk = configureModule();            // the module address comes from the key
}

}  // namespace longrange
