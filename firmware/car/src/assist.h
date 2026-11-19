// Obstacle assist for manual driving (phone, Bluetooth, remote, USB, LoRa).
// Plain C++ with no Arduino calls, so firmware/car/test/test_assist.cpp can
// check it on a PC.
//
// The agent has its own guards (agent/solar_rover_agent/safety.py and the
// 13 cm forward stop), trained against in the simulator, so this only runs for
// manual sources. It never steers and never touches reverse or turning on the
// spot: it only caps forward speed to what can still stop in time.
//
// Stopping model: the command reaches the wheels after ASSIST_LAG_S, then the
// car brakes at ASSIST_DECEL_MM_S2, and must end ASSIST_MARGIN_MM short of the
// obstacle. Solving  d - margin = v*lag + v^2 / (2*decel)  for v gives the
// fastest safe speed for a reading d.
#pragma once
#include <math.h>

constexpr float ASSIST_DECEL_MM_S2 = 1000.0f;  // well under the drive ramp (about 1800 mm/s^2)
constexpr float ASSIST_LAG_S       = 0.10f;    // sensor period + loop + motor response
constexpr float ASSIST_MARGIN_MM   = 60.0f;    // stop this far short (the nose overhangs the sensor a little)
constexpr float ASSIST_CORNER_COS  = 0.342f;   // cos(70 deg): a thing on a corner ray closes at v*cos(70)

// Fastest safe forward speed in mm/s for a reading of d mm (d < 0 = nothing seen).
inline float assistSafeSpeed(int d_mm) {
    if (d_mm < 0) return 1e9f;
    float room = (float)d_mm - ASSIST_MARGIN_MM;
    if (room <= 0) return 0;
    const float a = ASSIST_DECEL_MM_S2, t = ASSIST_LAG_S;
    return a * (-t + sqrtf(t * t + 2.0f * room / a));
}

struct AssistOut {
    float left, right;
    bool limiting;   // forward speed was cut
};

// left/right: the driver's tank command, -1..1. max_mm_s: wheel speed at 1.0.
// front, side_l, side_r: sensor readings in mm, -1 = nothing within range or not fitted.
// The corner readings only count on the side the driver is turning toward.
inline AssistOut assistLimit(float left, float right, int front, int side_l, int side_r, float max_mm_s) {
    float fwd = 0.5f * (left + right), turn = 0.5f * (right - left);   // turn > 0 = left
    if (fwd <= 0) return {left, right, false};
    float cap = assistSafeSpeed(front);
    int side = turn > 0.05f ? side_l : turn < -0.05f ? side_r : -1;
    if (side >= 0) {
        // driving straight, a thing on a corner ray passes beside the car; turning toward it,
        // the car closes on it at roughly v*cos(70 deg), so d on that ray is worth d/cos(70)
        // of straight-ahead room
        float c = assistSafeSpeed((int)(side / ASSIST_CORNER_COS));
        cap = fminf(cap, c);
    }
    float capCmd = cap / max_mm_s;
    if (fwd <= capCmd) return {left, right, false};
    float f = capCmd < 0 ? 0 : capCmd;
    return {f - turn, f + turn, true};
}
