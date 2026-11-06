// PC test for the long-range link logic (firmware/common/lora_link.h).
//   g++ -std=c++17 -Wall -Werror -Ifirmware/common firmware/car/test/test_lora_link.cpp -o /tmp/tll && /tmp/tll
#include "lora_link.h"
#include <cstdio>
#include <cstdlib>

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main() {
    using namespace lora;
    uint8_t key[16]; for (int i = 0; i < 16; i++) key[i] = (uint8_t)i;

    // SipHash-2-4 reference vectors (key 00..0f, message 00..n-1)
    uint8_t msg[64]; for (int i = 0; i < 64; i++) msg[i] = (uint8_t)i;
    CHECK(siphash24(key, msg, 0) == 0x726fdb47dd0e0e31ULL);
    CHECK(siphash24(key, msg, 15) == 0xa129ca6149be45e5ULL);
    CHECK(siphash24(key, msg, 8) == 0x93f5f5799a932462ULL);

    // frame sizes stay what the airtime numbers in docs/protocol.md assume
    CHECK(sizeof(Control) == 16 && sizeof(Ping) == 11 && sizeof(Status) == 16);

    // round trip, with and without the module's RSSI byte
    Frame f{}; f.c.lx = -40; f.c.ly = 100; f.c.buttons = 5; f.c.batt_pct = 77;
    size_t n = seal(f, F_CONTROL, 1234, key);
    uint8_t buf[32]; memcpy(buf, &f, n);
    Frame g{}; int rssi = 1;
    CHECK(open(buf, n, key, g, rssi) == F_CONTROL && rssi == 0);
    CHECK(g.c.lx == -40 && g.c.ly == 100 && g.c.buttons == 5 && g.h.counter == 1234);
    buf[n] = 256 - 87;
    CHECK(open(buf, n + 1, key, g, rssi) == F_CONTROL && rssi == -87);

    // a flipped bit, a wrong key, a truncated or padded packet are all refused
    buf[7] ^= 1; CHECK(open(buf, n, key, g, rssi) == 0); buf[7] ^= 1;
    uint8_t other[16]; memcpy(other, key, 16); other[15] ^= 0x80;
    CHECK(open(buf, n, other, g, rssi) == 0);
    CHECK(open(buf, n - 1, key, g, rssi) == 0);
    CHECK(open(buf, n + 2, key, g, rssi) == 0);
    buf[0] = 0; CHECK(open(buf, n, key, g, rssi) == 0);

    // replay: only rising counters pass
    Replay r; CHECK(r.accept(5)); CHECK(!r.accept(5)); CHECK(!r.accept(4)); CHECK(r.accept(6));

    // stick scaling
    CHECK(toByte(1000) == 100 && toByte(-1000) == -100 && toByte(55) == 5 && fromByte(-100) == -1000);

    // E220 setup bytes: 38400 8N1 + 9.6k air, RSSI byte on, US channel 65 at 22 dBm
    uint8_t cfg[CONFIG_LEN]; configFrame(cfg, Region::US915, key);
    CHECK(cfg[0] == 0xC0 && cfg[2] == 8 && cfg[3] == 0 && cfg[4] == 1);
    CHECK(cfg[5] == 0xA4 && cfg[6] == 0x00 && cfg[7] == 65 && cfg[8] == 0x83);
    configFrame(cfg, Region::EU868, key);
    CHECK(cfg[6] == 0x02 && cfg[7] == 18);
    uint8_t ok[CONFIG_LEN]; memcpy(ok, cfg, CONFIG_LEN); ok[0] = 0xC1; ok[9] = ok[10] = 0;
    CHECK(configAccepted(ok, CONFIG_LEN, cfg));
    ok[7] = 3; CHECK(!configAccepted(ok, CONFIG_LEN, cfg));
    uint8_t ff[16]; memset(ff, 0xFF, 16); configFrame(cfg, Region::US915, ff);
    CHECK(!(cfg[3] == 0xFF && cfg[4] == 0xFF));

    // airtime: a control frame at 9.6k is ~23 ms, so 10 Hz plus a status reply
    // every 5th frame fits in the 100 ms slot and in the US budget
    uint32_t ctl = airtimeUs(sizeof(Control)), st = airtimeUs(sizeof(Status));
    CHECK(ctl > 20000 && ctl < 26000);
    CHECK(ctl + 4000 + st < CONTROL_MS * 1000);
    CHECK(ctl * 1000 / CONTROL_MS < profile(Region::US915).duty * 1e6);

    // duty budget: EU 1 % runs dry under 10 Hz, then allows about one frame per 2.3 s
    DutyBudget b; b.set(profile(Region::EU868).duty);
    int sent = 0;
    for (uint32_t t = 0; t < 120000; t += 100) sent += b.trySpend(t, ctl);
    CHECK(sent > 50 && sent < 90);       // 600 ms of starting credit + 60 s at 10 ms/s
    DutyBudget us; us.set(profile(Region::US915).duty);
    sent = 0;
    for (uint32_t t = 0; t < 120000; t += 100) sent += us.trySpend(t, ctl);
    CHECK(sent == 1200);

    // failover: starts on LoRa, ESP-NOW takes over after 10 deliveries, LoRa
    // again 400 ms after the last delivery; one failure resets the streak
    Failover fo;
    uint32_t t = 0;
    CHECK(fo.longRange(t));
    for (int i = 0; i < 9; i++) fo.result(true, t += 20);
    CHECK(fo.longRange(t));
    fo.result(false, t += 20);
    for (int i = 0; i < 9; i++) fo.result(true, t += 20);
    CHECK(fo.longRange(t));
    fo.result(true, t += 20);
    CHECK(!fo.longRange(t));
    for (int i = 0; i < 19; i++) fo.result(false, t += 20);
    CHECK(!fo.longRange(t));
    t += 30; CHECK(fo.longRange(t));

    // car side: LoRa copies are ignored while ESP-NOW frames keep arriving
    CHECK(useLoraControl(1000, 0, false));
    CHECK(!useLoraControl(1000, 900, true));
    CHECK(useLoraControl(1000, 700, true));

    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("lora_link: all checks passed\n");
    return 0;
}
