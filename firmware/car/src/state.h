// Shared types for the car firmware.
#pragma once
#include <Arduino.h>
#include "config.h"

enum class Source : uint8_t { NONE = 0, USB, REMOTE, WEB, BLE, AGENT };
enum class Mode   : uint8_t { MANUAL = 0, AUTO, AGENT };   // AUTO: agent drives, manual input overrides

const char* sourceName(Source s);
const char* modeName(Mode m);
bool parseMode(const char* s, Mode& out);

struct PowerState {
    float batt_v = 0, charge_ma = 0, solar_v = 0, solar_mw = 0, temp_c = 25;
    float pct = 0;
    bool  low = false, critical = false, ntc_fault = false;
    bool  charger_on = false;
    const char* charge_state = "boot";   // off_temp | full | solar | wall | idle | fault
};

struct SensorState {
    float ax = 0, ay = 0, az = 0;   // g
    float gx = 0, gy = 0, gz = 0;   // deg/s (bias removed)
    int   tof_mm = -1;              // -1 = no reading
    bool  imu_ok = false, tof_ok = false;
};

struct DriveState {
    long  enc[4] = {0, 0, 0, 0};     // signed edge counts FL, RL, FR, RR
    float speed_mm_s[4] = {0, 0, 0, 0};
    float duty[4] = {0, 0, 0, 0};
    float cmd_left = 0, cmd_right = 0;  // -1..1
};

extern PowerState  gPower;
extern SensorState gSensors;
extern DriveState  gDrive;
