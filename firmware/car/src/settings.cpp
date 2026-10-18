#include "settings.h"
#include <Preferences.h>

namespace {
Preferences prefs;
uint32_t pairUntil = 0;
bool restart = false;
String ip = "-";
}

namespace settings {
void begin() {
    prefs.begin("solarrover", false);
    pairUntil = millis() + 60000;   // first minute after power-on: pairing open if no remote stored
    uint8_t mac[6];
    if (remoteMac(mac)) pairUntil = 0;
}
void saveWifi(const char* s, const char* p) { prefs.putString("ssid", s); prefs.putString("pass", p); }
String wifiSsid() { return prefs.getString("ssid", ""); }
String wifiPass() { return prefs.getString("pass", ""); }
void saveCal(int j, float o, int d) {
    char k[8];
    snprintf(k, sizeof k, "co%d", j); prefs.putFloat(k, o);
    snprintf(k, sizeof k, "cd%d", j); prefs.putChar(k, d >= 0 ? 1 : -1);
}
bool loadCal(int j, float& o, int& d) {
    char k[8]; snprintf(k, sizeof k, "co%d", j);
    if (!prefs.isKey(k)) return false;
    o = prefs.getFloat(k, 90);
    snprintf(k, sizeof k, "cd%d", j); d = prefs.getChar(k, 1);
    return true;
}
void startPairing() { pairUntil = millis() + 60000; }
bool pairing() { return pairUntil && (int32_t)(pairUntil - millis()) > 0; }
bool remoteMac(uint8_t mac[6]) { return prefs.getBytes("rmac", mac, 6) == 6; }
void saveRemoteMac(const uint8_t mac[6]) { prefs.putBytes("rmac", mac, 6); pairUntil = 0; }
void requestRestart() { restart = true; }
bool restartRequested() { return restart; }
void setIp(const String& s) { ip = s; }
String ipString() { return ip; }
}
