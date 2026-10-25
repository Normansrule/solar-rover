// Long-range link to the handheld remote over an E220 LoRa module
// (firmware/common/lora_link.h has the frames and the rules).
#pragma once
#include "state.h"

namespace longrange {
void begin();                      // set the module up; call after WiFi is on (key uses the RF noise RNG)
void poll();                       // read frames from the module (main loop)
void sleep();                      // module to sleep mode (low battery)
bool ok();                         // module answered at setup
int lastRssi();                    // dBm of the last good frame, 0 = none yet
uint32_t lastFrameMs();            // millis() of the last good frame
void fillKeyPacket(uint8_t* out, size_t& n);   // the ESP-NOW pairing packet (LoraKeyPacket); resets the replay floor
void setRegion(uint8_t r);         // saves it and sets the module up again
void rekey();                      // new key: the remote must pair again
const char* regionName();
}
