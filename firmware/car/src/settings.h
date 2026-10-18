// Persistent settings (NVS): WiFi credentials, paired remote, servo calibration.
#pragma once
#include <Arduino.h>

namespace settings {
void begin();
void saveWifi(const char* ssid, const char* pass);
String wifiSsid();
String wifiPass();
void saveCal(int joint, float offset, int dir);
bool loadCal(int joint, float& offset, int& dir);
void startPairing();
bool pairing();
bool remoteMac(uint8_t mac[6]);      // false if none paired
void saveRemoteMac(const uint8_t mac[6]);
void requestRestart();
bool restartRequested();
void setIp(const String& ip);
String ipString();
}
