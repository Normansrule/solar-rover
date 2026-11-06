// Judging one wheel of the bring-up wheel test (TEST WHEELS, see bringup.cpp).
// Plain C++ with no Arduino calls, so firmware/car/test/test_wheel_check.cpp
// can check it on a PC.
//
// The slot encoders are single-channel: they count edges but can't tell which
// way the wheel turns. The gyro can. Driving one wheel forward on the floor
// turns the car: a left wheel turns it right (yaw rate negative, z up), a right
// wheel turns it left. So the gyro gives the direction and the encoders show
// which wheel actually moved.
#pragma once
#include <math.h>

enum class WheelVerdict {
    OK,            // turned the right way and its own encoder counted
    REVERSED,      // turned backwards: swap this motor's two wires
    ENC_SWAPPED,   // moved, but another wheel's encoder input counted it
    ENC_MISSING,   // the car turned but no encoder counted
    NO_MOTION,     // nothing moved and nothing counted
    DIR_UNKNOWN,   // encoder counted but the car didn't turn (lifted, or no IMU)
};

struct WheelRun {
    long  edges[4];   // raw encoder edges of FL, RL, FR, RR while this wheel ran
    float yaw_deg;    // gyro z integrated over the run, CCW positive
};

struct WheelResult {
    WheelVerdict verdict;
    int counted_by;   // index of the encoder that counted most (-1 if none)
};

constexpr long  WHEEL_MIN_EDGES = 8;      // ~1/5 turn in the 0.7 s run
constexpr float WHEEL_MIN_YAW_DEG = 4.0f; // a single wheel turns the car ~15-40 deg

inline WheelResult judgeWheel(int wheel, const WheelRun& r, bool imuOk) {
    int best = -1;
    long most = 0;
    for (int i = 0; i < 4; i++)
        if (r.edges[i] > most) { most = r.edges[i]; best = i; }
    bool turned = imuOk && fabsf(r.yaw_deg) >= WHEEL_MIN_YAW_DEG;
    if (most < WHEEL_MIN_EDGES)
        return {turned ? WheelVerdict::ENC_MISSING : WheelVerdict::NO_MOTION, -1};
    if (best != wheel) return {WheelVerdict::ENC_SWAPPED, best};
    if (!turned) return {WheelVerdict::DIR_UNKNOWN, best};
    bool leftWheel = wheel < 2;
    bool turnedRight = r.yaw_deg < 0;
    return {turnedRight == leftWheel ? WheelVerdict::OK : WheelVerdict::REVERSED, best};
}

inline const char* wheelVerdictText(WheelVerdict v) {
    switch (v) {
        case WheelVerdict::OK:          return "ok";
        case WheelVerdict::REVERSED:    return "reversed: swap this motor's two wires at the TB6612";
        case WheelVerdict::ENC_SWAPPED: return "encoder swapped";
        case WheelVerdict::ENC_MISSING: return "no encoder: check the LM393 jumper and the slot disc";
        case WheelVerdict::NO_MOTION:   return "did not move: check the motor wires, STBY and battery";
        case WheelVerdict::DIR_UNKNOWN: return "turns, direction unknown: put the car on the floor and check it rolls forward";
    }
    return "?";
}

// All four "reversed" almost always means the IMU is upside down, not that
// every motor is wired backwards.
inline bool imuLooksFlipped(const WheelResult res[4]) {
    for (int i = 0; i < 4; i++) if (res[i].verdict != WheelVerdict::REVERSED) return false;
    return true;
}
