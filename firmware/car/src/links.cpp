#include "links.h"
#include "protocol.h"
#include "control.h"
#include "settings.h"
#include "web_index.h"
#include "../../common/remote_packet.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ESPAsyncWebServer.h>
#include <NimBLEDevice.h>

namespace {
struct Msg { Source src; char line[120]; };
QueueHandle_t queue;
HardwareSerial& piSerial = Serial2;
String usbBuf, piBuf;
bool radiosDown = false;
uint32_t lastUartTx = 0, lastAppTx = 0;

// ---------------- WiFi + web app ----------------
AsyncWebServer server(WEB_PORT);
AsyncWebSocket ws("/ws");

void onWs(AsyncWebSocket*, AsyncWebSocketClient* c, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type != WS_EVT_DATA) return;
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return;
    char tmp[120];
    size_t n = min(len, sizeof(tmp) - 1);
    memcpy(tmp, data, n); tmp[n] = 0;
    // a frame may carry several newline-separated commands
    char* save = nullptr;
    for (char* l = strtok_r(tmp, "\n", &save); l; l = strtok_r(nullptr, "\n", &save)) links::enqueue(Source::WEB, l);
}

void startWifi() {
    uint8_t mac[6]; WiFi.macAddress(mac);
    char apName[24]; snprintf(apName, sizeof apName, "SolarRover-%02X%02X", mac[4], mac[5]);
    String ssid = settings::wifiSsid();
    WiFi.mode(ssid.length() ? WIFI_AP_STA : WIFI_AP);
    WiFi.softAP(apName, AP_PASSWORD);
    if (ssid.length()) {
        WiFi.begin(ssid.c_str(), settings::wifiPass().c_str());
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(100);
    }
    String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    settings::setIp(ip);
    MDNS.begin("solar-rover");
    ws.onEvent(onWs);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
        AsyncWebServerResponse* res = r->beginResponse(200, "text/html", WEB_INDEX_GZ, WEB_INDEX_GZ_LEN);
        res->addHeader("Content-Encoding", "gzip");
        r->send(res);
    });
    server.begin();
    MDNS.addService("http", "tcp", WEB_PORT);
    Serial.printf("WiFi AP %s (pass %s), app at http://%s/ or http://solar-rover.local/\n", apName, AP_PASSWORD, ip.c_str());
}

// ---------------- BLE (Nordic UART service) ----------------
const char* NUS_SVC = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
const char* NUS_RX  = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";   // phone -> car
const char* NUS_TX  = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";   // car -> phone
NimBLECharacteristic* bleTx = nullptr;
bool bleConnected = false;
String bleBuf;

class RxCb : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        std::string v = c->getValue();
        for (char ch : v) {
            if (ch == '\n' || ch == '\r') { if (bleBuf.length()) links::enqueue(Source::BLE, bleBuf.c_str()); bleBuf = ""; }
            else if (bleBuf.length() < 110) bleBuf += ch;
        }
        // Bluefruit Connect sends packets without a newline
        if (bleBuf.startsWith("!B") && bleBuf.length() >= 5) { links::enqueue(Source::BLE, bleBuf.c_str()); bleBuf = ""; }
    }
};
class SrvCb : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*) override { bleConnected = true; }
    void onDisconnect(NimBLEServer*) override { bleConnected = false; NimBLEDevice::startAdvertising(); }
};

void startBle() {
    NimBLEDevice::init(BLE_NAME);
    NimBLEDevice::setMTU(247);
    NimBLEServer* srv = NimBLEDevice::createServer();
    srv->setCallbacks(new SrvCb());
    NimBLEService* svc = srv->createService(NUS_SVC);
    bleTx = svc->createCharacteristic(NUS_TX, NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic* rx = svc->createCharacteristic(NUS_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    rx->setCallbacks(new RxCb());
    svc->start();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(NUS_SVC);
    adv->setScanResponse(true);
    adv->start();
}

void bleSend(const char* s, size_t n) {
    if (!bleConnected || !bleTx) return;
    size_t mtu = NimBLEDevice::getMTU();
    size_t chunk = mtu > 23 ? mtu - 3 : 20;
    for (size_t i = 0; i < n; i += chunk) { bleTx->setValue((const uint8_t*)s + i, min(chunk, n - i)); bleTx->notify(); }
}

// ---------------- ESP-NOW handheld remote ----------------
QueueHandle_t remoteQ;
uint8_t remoteAddr[6]; bool remoteKnown = false;

void onEspNowRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(RemotePacket)) return;
    RemotePacket p; memcpy(&p, data, sizeof p);
    if (p.magic != RC_MAGIC) return;
    if (remoteKnown && memcmp(mac, remoteAddr, 6) != 0) return;
    if (!remoteKnown) {
        if (!settings::pairing() || p.type != PKT_HELLO) return;
        memcpy(remoteAddr, mac, 6); remoteKnown = true; settings::saveRemoteMac(mac);
    }
    if (p.type == PKT_HELLO) {
        if (!esp_now_is_peer_exist(mac)) {
            esp_now_peer_info_t peer = {}; memcpy(peer.peer_addr, mac, 6); peer.channel = 0; peer.encrypt = false;
            esp_now_add_peer(&peer);
        }
        uint8_t ch; wifi_second_chan_t sc; esp_wifi_get_channel(&ch, &sc);
        CarAck a{RC_MAGIC, PKT_ACK, ch, (uint8_t)gPower.pct, (uint8_t)control::mode()};
        esp_now_send(mac, (uint8_t*)&a, sizeof a);
        return;
    }
    xQueueSend(remoteQ, &p, 0);
}

void startEspNow() {
    remoteQ = xQueueCreate(4, sizeof(RemotePacket));
    remoteKnown = settings::remoteMac(remoteAddr);
    if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); return; }
    esp_now_register_recv_cb(onEspNowRecv);
    if (remoteKnown) {
        esp_now_peer_info_t peer = {}; memcpy(peer.peer_addr, remoteAddr, 6); peer.channel = 0; peer.encrypt = false;
        esp_now_add_peer(&peer);
    }
}

void handleRemote(const RemotePacket& p) {
    static uint8_t prevBtn = 0;
    static bool gripOpen = false, ready = false;
    uint8_t pressed = p.buttons & ~prevBtn;
    prevBtn = p.buttons;
    if ((p.buttons & (BTN_LCLICK | BTN_RCLICK)) == (BTN_LCLICK | BTN_RCLICK)) { control::stop(Source::REMOTE); return; }
    if (pressed & BTN_LCLICK) {
        Mode m = control::mode();
        control::setMode(Source::REMOTE, m == Mode::MANUAL ? Mode::AUTO : m == Mode::AUTO ? Mode::AGENT : Mode::MANUAL);
    }
    if (pressed & BTN_RCLICK) { ready = !ready; control::armPose(Source::REMOTE, ready ? "ready" : "park"); }
    if (pressed & BTN_A) { gripOpen = !gripOpen; control::grip(Source::REMOTE, gripOpen ? 90 : 0); }
    control::arcade(Source::REMOTE, p.ly / 1000.0f, -p.lx / 1000.0f);
    bool alt = p.buttons & BTN_B;
    int jx = alt ? J_WRIST : J_YAW, jy = alt ? J_ELBOW : J_SHOULDER;
    control::armJog(Source::REMOTE, jx, -p.rx / 1000.0f * JOINTS[jx].max_speed_dps);
    control::armJog(Source::REMOTE, jy, (alt ? -1 : 1) * p.ry / 1000.0f * JOINTS[jy].max_speed_dps);
    if (!p.lx && !p.ly && !p.rx && !p.ry && !p.buttons) control::heartbeat(Source::REMOTE);
}

void reply(Source s, const String& r) {
    if (!r.length()) return;
    switch (s) {
        case Source::USB: Serial.println(r); break;
        case Source::AGENT: piSerial.println(r); break;
        case Source::WEB: ws.textAll(r); break;
        case Source::BLE: { String x = r + "\n"; bleSend(x.c_str(), x.length()); } break;
        default: break;
    }
}

void readLines(Stream& in, String& buf, Source src) {
    while (in.available()) {
        char c = in.read();
        if (c == '\n' || c == '\r') { if (buf.length()) { String r; protocol::handle(src, buf.c_str(), r); reply(src, r); } buf = ""; }
        else if (buf.length() < 150) buf += c;
    }
}
}  // namespace

namespace links {

void enqueue(Source src, const char* line) {
    Msg m; m.src = src; strlcpy(m.line, line, sizeof m.line);
    xQueueSend(queue, &m, 0);
}

void begin() {
    queue = xQueueCreate(16, sizeof(Msg));
    piSerial.begin(PI_BAUD, SERIAL_8N1, PIN_PI_RX, PIN_PI_TX);
    startWifi();
    startEspNow();
    startBle();
}

void poll() {
    readLines(Serial, usbBuf, Source::USB);
    readLines(piSerial, piBuf, Source::AGENT);
    Msg m;
    while (xQueueReceive(queue, &m, 0) == pdTRUE) { String r; protocol::handle(m.src, m.line, r); reply(m.src, r); }
    RemotePacket p;
    if (remoteQ) while (xQueueReceive(remoteQ, &p, 0) == pdTRUE) handleRemote(p);
    ws.cleanupClients();
}

void sendTelemetry() {
    static char buf[640];
    uint32_t now = millis();
    if (now - lastUartTx >= TELEMETRY_UART_MS) {
        lastUartTx = now;
        size_t n = protocol::telemetry(buf, sizeof buf);
        piSerial.write((const uint8_t*)buf, n); piSerial.write('\n');
    }
    if (now - lastAppTx >= TELEMETRY_APP_MS && !radiosDown) {
        lastAppTx = now;
        size_t n = protocol::telemetry(buf, sizeof buf);
        if (ws.count()) ws.textAll(buf, n);
        buf[n] = '\n';
        bleSend(buf, n + 1);
    }
}

void sendToPi(const char* line) { piSerial.println(line); }
void send(Source to, const String& line) { reply(to, line); }

void radiosOff() {
    if (radiosDown) return;
    radiosDown = true;
    NimBLEDevice::deinit(true);
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
}
bool radiosAreOff() { return radiosDown; }

}  // namespace links
