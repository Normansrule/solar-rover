// All the ways commands reach the car. Callbacks running on radio tasks only
// enqueue; links::poll() hands everything to the protocol on the main loop.
#pragma once
#include "state.h"
struct RemotePacket;

namespace links {
void begin();
void poll();                                  // process queued input (main loop)
void sendTelemetry();                         // rate-limited per link
void sendToPi(const char* line);              // e.g. "SHUTDOWN"
void radiosOff();                             // low-power: WiFi + BLE off
bool radiosAreOff();
void enqueue(Source src, const char* line);   // thread-safe
void remoteInput(const RemotePacket& p, Source src);   // handheld remote input from LoRa (main loop)
}
