// Long-range link between the handheld remote and the car: two Ebyte
// E220-900T22D LoRa modules (UART, transparent mode). Shared by both firmware
// projects and by the PC test (firmware/car/test/test_lora_link.cpp), so it is
// plain C++ with no Arduino calls. See docs/protocol.md, "Long-range link".
//
// - Frames are small fixed-size structs, signed with a 32-bit SipHash-2-4 tag
//   under a 16-byte key the car hands the remote over ESP-NOW while pairing.
//   A counter that only goes up stops recorded frames being played back.
// - The remote talks first; the car only answers (half duplex, no collisions).
// - The remote sends over LoRa when ESP-NOW stops getting through (Failover).
// - An airtime budget keeps each region's duty-cycle rule (DutyBudget).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace lora {

// ---------------- radio settings ----------------
enum class Region : uint8_t { US915 = 0, EU868 = 1 };

struct Profile {
    const char* name;
    uint8_t channel;        // E220-900: frequency = 850.125 MHz + channel
    uint8_t power_code;     // REG1 bits 1..0: 0 = 22 dBm, 1 = 17, 2 = 13, 3 = 10
    float duty;             // share of airtime one radio may use (rolling)
    bool drive_ok;          // false: the budget is too small to drive, only buttons get through
};

// US: 915.125 MHz, 22 dBm. EU: 868.125 MHz (g1 sub-band: 25 mW e.r.p., 1 % duty),
// 13 dBm, which leaves room for about one frame every 2 s, so only STOP, mode
// and arm-pose buttons go over LoRa there, not the sticks.
inline Profile profile(Region r) {
    if (r == Region::EU868) return {"EU868", 18, 2, 0.01f, false};
    return {"US915", 65, 0, 0.5f, true};
}
inline bool parseRegion(const char* s, Region& out) {
    if (!s) return false;
    if ((s[0] == 'U' || s[0] == 'u') && (s[1] == 'S' || s[1] == 's')) { out = Region::US915; return true; }
    if ((s[0] == 'E' || s[0] == 'e') && (s[1] == 'U' || s[1] == 'u')) { out = Region::EU868; return true; }
    return false;
}

constexpr uint32_t AIR_BPS      = 9600;    // E220 "air data rate"; 9.6k is the range/latency middle
constexpr uint8_t  AIR_CODE     = 0b100;   // REG0 bits 2..0 for 9.6k
constexpr uint32_t UART_BAUD    = 38400;   // after setup (setup itself is always 9600)
constexpr uint8_t  UART_CODE    = 0b101;   // REG0 bits 7..5 for 38400
constexpr uint32_t CONTROL_MS   = 100;     // remote -> car while driving over LoRa (10 Hz)
constexpr uint32_t PING_MS      = 1000;    // remote -> car while ESP-NOW is fine (keeps the link measured)
constexpr uint8_t  STATUS_EVERY = 5;       // car answers every 5th control frame, and every ping
constexpr uint32_t RX_GAP_MS    = 4;       // UART quiet this long = end of a received packet

// E220 register write (mode 3, 9600 8N1): C0 00 08 ADDH ADDL REG0 REG1 REG2 REG3 CRYPT_H CRYPT_L.
// The module address comes from the key, so two rovers near each other don't
// even hear each other's frames. The module answers C1 00 08 + the same 8 bytes.
constexpr size_t CONFIG_LEN = 11;
inline size_t configFrame(uint8_t out[CONFIG_LEN], Region r, const uint8_t key[16]) {
    Profile p = profile(r);
    uint8_t addh = key[0], addl = key[1];
    if (addh == 0xFF && addl == 0xFF) addl = 0xFE;      // FFFF is the broadcast/monitor address
    out[0] = 0xC0; out[1] = 0x00; out[2] = 0x08;
    out[3] = addh; out[4] = addl;
    out[5] = (uint8_t)(UART_CODE << 5) | (0b00 << 3) | AIR_CODE;   // 38400 baud, 8N1, air rate
    out[6] = (uint8_t)(p.power_code & 0x03);                       // 200-byte sub-packets, no noise RSSI
    out[7] = p.channel;
    out[8] = 0x80 | 0x03;                                          // RSSI byte after each packet, transparent, no LBT, WOR 2 s
    out[9] = key[2]; out[10] = key[3];                             // the module's own scrambling key (weak; the tag is what counts)
    return CONFIG_LEN;
}
inline bool configAccepted(const uint8_t* reply, size_t n, const uint8_t sent[CONFIG_LEN]) {
    // the module never reads the crypt key back (it answers 00 00), so compare up to REG3
    if (n < CONFIG_LEN || reply[0] != 0xC1 || reply[1] != 0x00 || reply[2] != 0x08) return false;
    return memcmp(reply + 3, sent + 3, 6) == 0;
}

// Rough airtime of one packet: payload plus ~12 bytes' worth of preamble,
// header and CRC at the air data rate. Good enough for a duty-cycle budget.
inline uint32_t airtimeUs(size_t bytes, uint32_t bps = AIR_BPS) {
    return (uint32_t)(((uint64_t)(bytes + 12) * 8u * 1000000u) / bps);
}

// ---------------- SipHash-2-4 ----------------
inline uint64_t rd64(const uint8_t* p) { uint64_t v = 0; for (int i = 7; i >= 0; i--) v = (v << 8) | p[i]; return v; }
inline uint64_t rotl(uint64_t x, int b) { return (x << b) | (x >> (64 - b)); }
inline uint64_t siphash24(const uint8_t key[16], const uint8_t* m, size_t n) {
    uint64_t k0 = rd64(key), k1 = rd64(key + 8);
    uint64_t v0 = 0x736f6d6570736575ULL ^ k0, v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0, v3 = 0x7465646279746573ULL ^ k1;
    auto round = [&] {
        v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
        v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
        v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
        v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
    };
    size_t end = n - n % 8;
    for (size_t i = 0; i < end; i += 8) { uint64_t mi = rd64(m + i); v3 ^= mi; round(); round(); v0 ^= mi; }
    uint64_t b = (uint64_t)n << 56;
    for (size_t i = 0; i < n % 8; i++) b |= (uint64_t)m[end + i] << (8 * i);
    v3 ^= b; round(); round(); v0 ^= b;
    v2 ^= 0xff; round(); round(); round(); round();
    return v0 ^ v1 ^ v2 ^ v3;
}

// ---------------- frames ----------------
constexpr uint8_t SYNC = 0xA7;
enum : uint8_t { F_CONTROL = 1, F_PING = 2, F_STATUS = 3 };
enum : uint8_t { ST_LOW = 1, ST_CRIT = 2, ST_CHARGING = 4, ST_LORA_DRIVES = 8 };

struct __attribute__((packed)) Header { uint8_t sync, type; uint32_t counter; };
struct __attribute__((packed)) Control {          // remote -> car, 16 bytes
    Header h;
    int8_t lx, ly, rx, ry;                         // sticks, -100..100 (the ESP-NOW packet's /10)
    uint8_t buttons, batt_pct;                     // BTN_* bits from remote_packet.h
    uint8_t tag[4];
};
struct __attribute__((packed)) Ping {             // remote -> car, 11 bytes
    Header h;
    uint8_t batt_pct;
    uint8_t tag[4];
};
struct __attribute__((packed)) Status {           // car -> remote, 16 bytes
    Header h;
    uint8_t batt_pct, mode, flags;                 // mode 0 manual, 1 auto, 2 agent; ST_* flags
    int8_t rssi;                                   // dBm of the last frame the car heard
    int16_t speed_mm_s;                            // mean wheel speed
    uint8_t tag[4];
};
union Frame { Header h; Control c; Ping p; Status s; };

inline size_t frameSize(uint8_t type) {
    switch (type) {
        case F_CONTROL: return sizeof(Control);
        case F_PING: return sizeof(Ping);
        case F_STATUS: return sizeof(Status);
        default: return 0;
    }
}

// Fill in the header and the tag. Returns the number of bytes to send.
inline size_t seal(Frame& f, uint8_t type, uint32_t counter, const uint8_t key[16]) {
    size_t n = frameSize(type);
    f.h.sync = SYNC; f.h.type = type; f.h.counter = counter;
    uint32_t t = (uint32_t)siphash24(key, (const uint8_t*)&f, n - 4);
    memcpy((uint8_t*)&f + n - 4, &t, 4);
    return n;
}

// Check one received packet (the bytes between two UART gaps). The module adds
// one RSSI byte at the end. Returns the frame type, or 0 when the packet is
// not a valid frame under this key. Does not check the counter: see Replay.
inline uint8_t open(const uint8_t* buf, size_t n, const uint8_t key[16], Frame& out, int& rssi_dbm) {
    if (n < 2 || buf[0] != SYNC) return 0;
    size_t want = frameSize(buf[1]);
    if (!want || (n != want && n != want + 1)) return 0;
    uint32_t t = (uint32_t)siphash24(key, buf, want - 4);
    if (memcmp(&t, buf + want - 4, 4) != 0) return 0;
    memcpy(&out, buf, want);
    rssi_dbm = n == want + 1 ? -(256 - (int)buf[want]) : 0;
    return buf[1];
}

// Counters only go up. Call accept() only after open() said the tag is good.
struct Replay {
    uint32_t last = 0;
    bool accept(uint32_t c) { if (c <= last) return false; last = c; return true; }
};

inline int8_t toByte(int16_t v) { int x = v / 10; return (int8_t)(x > 100 ? 100 : x < -100 ? -100 : x); }
inline int16_t fromByte(int8_t v) { int x = v > 100 ? 100 : v < -100 ? -100 : v; return (int16_t)(x * 10); }

// ---------------- link choice and budgets ----------------
// On the remote: ESP-NOW's own delivery reports decide which link drives.
// After down_ms without a delivered packet, drive over LoRa; go back after
// up_count deliveries in a row, so a flaky edge of range doesn't flap.
struct Failover {
    uint32_t down_ms = 400;
    uint8_t up_count = 10;
    uint32_t lastOk = 0;
    uint8_t streak = 0;
    bool lr = true;               // until ESP-NOW proves itself
    void result(bool ok, uint32_t now) {
        if (ok) { lastOk = now; if (streak < 255) streak++; if (streak >= up_count) lr = false; }
        else streak = 0;
    }
    bool longRange(uint32_t now) {
        if (!lr && now - lastOk > down_ms) { lr = true; streak = 0; }
        return lr;
    }
};

// On the car: a LoRa control frame is a copy of what ESP-NOW already brought
// while ESP-NOW is still arriving, so it is only used once ESP-NOW goes quiet.
constexpr uint32_t ESPNOW_FRESH_MS = 250;
inline bool useLoraControl(uint32_t now, uint32_t lastEspNowMs, bool espNowEverSeen) {
    return !espNowEverSeen || now - lastEspNowMs > ESPNOW_FRESH_MS;
}

// Rolling airtime budget: credit refills at `duty` of real time, up to one
// minute's worth, and each packet spends its airtime.
struct DutyBudget {
    float duty = 1.0f;
    uint32_t cap_us = 0, credit_us = 0, lastMs = 0;
    bool started = false;
    void set(float d, uint32_t capWindowMs = 60000) {
        duty = d; cap_us = (uint32_t)(d * capWindowMs * 1000.0f); credit_us = cap_us; started = false;
    }
    void refill(uint32_t now) {
        if (!started) { started = true; lastMs = now; return; }
        uint64_t add = (uint64_t)((now - lastMs) * 1000.0f * duty);
        lastMs = now;
        credit_us = (uint32_t)((uint64_t)credit_us + add > cap_us ? cap_us : credit_us + add);
    }
    bool trySpend(uint32_t now, uint32_t air_us) {
        refill(now);
        if (air_us > credit_us) return false;
        credit_us -= air_us;
        return true;
    }
};

}  // namespace lora
