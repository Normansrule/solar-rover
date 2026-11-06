// Persistent settings (NVS): WiFi credentials, paired remote, servo calibration, LoRa key.
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
void loraKey(uint8_t key[16]);        // made from the hardware RNG on first boot
void newLoraKey();                    // LORA REKEY: the remote needs pairing again
uint8_t loraRegion();                 // lora::Region
void setLoraRegion(uint8_t r);
uint32_t loraCounter();               // replay floor: highest remote counter seen (saved every 256)
void saveLoraCounter(uint32_t c);
void requestRestart();
bool restartRequested();
void setIp(const String& ip);
String ipString();
}
