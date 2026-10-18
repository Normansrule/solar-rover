// ESP-NOW packets between the handheld remote and the car. Shared by both
// firmware projects; keep them identical.
#pragma once
#include <stdint.h>

constexpr uint32_t RC_MAGIC = 0x52434152;   // "RCAR"
enum : uint8_t { PKT_CONTROL = 1, PKT_HELLO = 2, PKT_ACK = 3 };
enum : uint8_t { BTN_LCLICK = 1, BTN_RCLICK = 2, BTN_A = 4, BTN_B = 8 };

struct __attribute__((packed)) RemotePacket {
    uint32_t magic;
    uint8_t  type;        // PKT_CONTROL or PKT_HELLO
    uint8_t  seq;
    int16_t  lx, ly, rx, ry;   // -1000..1000 after dead zone, + = right / up
    uint8_t  buttons;
    uint8_t  batt_pct;
};

struct __attribute__((packed)) CarAck {
    uint32_t magic;
    uint8_t  type;        // PKT_ACK
    uint8_t  channel;
    uint8_t  batt_pct;    // car battery, shown on the remote LED
    uint8_t  mode;        // 0 manual, 1 auto, 2 agent
};
