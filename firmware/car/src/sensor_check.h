// Judging the bring-up sensor test (TEST SENSORS, see bringup.cpp): two seconds
// with the car standing still, then one line per sensor saying whether it
// answered and whether its reading makes sense for a car at rest on the floor.
// Plain C++ with no Arduino calls, so firmware/car/test/test_sensor_check.cpp
// can check it on a PC.
#pragma once
#include <math.h>
#include <stdio.h>

enum class Check { OK, WARN, FAIL, ABSENT };   // ABSENT = optional part not fitted

struct SensorSummary {
    // MPU-6050: means over the test, g and deg/s
    bool imu_ok;
    float ax, ay, az, gyro_abs;          // gyro_abs = mean |gx|+|gy|+|gz| (bias removed)
    // distance sensors: front, left corner, right corner
    bool tof_ok[3];
    int  tof_valid[3], tof_samples[3];   // readings within range / all readings
    int  tof_min[3], tof_max[3];         // mm, over the valid readings
    bool mux;                            // TCA9548A answered
    // power
    bool batt_ok, solar_ok;
    float batt_v, solar_v;
    bool temp_fault; float temp_c;
    bool pca_ok, lora_ok;
};

struct CheckLine { Check level; char text[96]; };

constexpr float SC_TILT_WARN_DEG = 10.0f;   // more than this off level: the IMU isn't flat
constexpr float SC_GYRO_STILL_DPS = 3.0f;   // more than this while standing still: it moved, or a bad gyro
constexpr int   SC_TOF_COVERED_MM = 30;     // closer than this all the time: the sensor is covered

inline CheckLine scImu(const SensorSummary& s) {
    CheckLine l{Check::OK, ""};
    if (!s.imu_ok) { l.level = Check::FAIL; snprintf(l.text, sizeof l.text, "missing: check its I2C hub lead (0x68, AD0 to GND)"); return l; }
    float g = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
    if (s.az < -0.5f) { l.level = Check::FAIL; snprintf(l.text, sizeof l.text, "upside down (z reads %.2f g): mount it chip side up", s.az); return l; }
    if (g < 0.8f || g > 1.2f) { l.level = Check::FAIL; snprintf(l.text, sizeof l.text, "reads %.2f g at rest, should be 1.00: faulty board?", g); return l; }
    float tilt = acosf(fminf(1.0f, s.az / g)) * 57.2958f;
    if (tilt > SC_TILT_WARN_DEG) { l.level = Check::WARN; snprintf(l.text, sizeof l.text, "tilted %.0f deg: mount it flat, or put the car on level ground", tilt); return l; }
    if (s.gyro_abs > SC_GYRO_STILL_DPS) { l.level = Check::WARN; snprintf(l.text, sizeof l.text, "gyro reads %.1f deg/s standing still: keep the car still and restart", s.gyro_abs); return l; }
    snprintf(l.text, sizeof l.text, "ok, level to %.0f deg", tilt);
    return l;
}

inline CheckLine scTof(const SensorSummary& s, int i) {
    static const char* where[3] = {"front", "left corner", "right corner"};
    CheckLine l{Check::OK, ""};
    if (!s.tof_ok[i]) {
        if (i > 0 && !s.mux) { l.level = Check::ABSENT; snprintf(l.text, sizeof l.text, "not fitted (no TCA9548A mux): corner guard and assist corners off"); }
        else { l.level = Check::FAIL; snprintf(l.text, sizeof l.text, "%s sensor missing: check its lead on mux channel %d", where[i], i); }
        return l;
    }
    if (s.tof_samples[i] == 0) { l.level = Check::FAIL; snprintf(l.text, sizeof l.text, "answered at boot but gave no readings"); return l; }
    if (s.tof_valid[i] == 0) { snprintf(l.text, sizeof l.text, "ok, nothing within 2 m"); return l; }
    if (s.tof_max[i] < SC_TOF_COVERED_MM) {
        l.level = Check::WARN;
        snprintf(l.text, sizeof l.text, "reads %d mm all the time: peel off the protective film, or the window is blocked", s.tof_max[i]);
        return l;
    }
    snprintf(l.text, sizeof l.text, "ok, %d cm", (s.tof_min[i] + s.tof_max[i]) / 20);
    return l;
}

inline CheckLine scBattery(const SensorSummary& s) {
    CheckLine l{Check::OK, ""};
    if (!s.batt_ok) { l.level = Check::FAIL; snprintf(l.text, sizeof l.text, "INA219 0x41 missing: bridge its A0 pads, check the hub lead"); return l; }
    if (s.batt_v < 6.0f) { l.level = Check::FAIL; snprintf(l.text, sizeof l.text, "%.2f V: pack flat or not wired through this INA219", s.batt_v); return l; }
    if (s.batt_v > 8.6f) { l.level = Check::FAIL; snprintf(l.text, sizeof l.text, "%.2f V is too high for 2S: check the pack", s.batt_v); return l; }
    if (s.batt_v < 6.8f) { l.level = Check::WARN; snprintf(l.text, sizeof l.text, "%.2f V: nearly empty, charge it", s.batt_v); return l; }
    snprintf(l.text, sizeof l.text, "ok, %.2f V", s.batt_v);
    return l;
}

inline CheckLine scSolar(const SensorSummary& s) {
    CheckLine l{Check::OK, ""};
    if (!s.solar_ok) { l.level = Check::FAIL; snprintf(l.text, sizeof l.text, "INA219 0x44 missing: bridge its A1 pads, check the hub lead"); return l; }
    if (s.solar_v < 0.5f) snprintf(l.text, sizeof l.text, "ok, panels dark (%.1f V): fine indoors", s.solar_v);
    else snprintf(l.text, sizeof l.text, "ok, panels %.1f V", s.solar_v);
    return l;
}

inline CheckLine scTemp(const SensorSummary& s) {
    CheckLine l{Check::OK, ""};
    if (s.temp_fault) { l.level = Check::FAIL; snprintf(l.text, sizeof l.text, "DS18B20 probe not found: charging stays off until it is"); return l; }
    if (s.temp_c < 0 || s.temp_c > 45) { l.level = Check::WARN; snprintf(l.text, sizeof l.text, "%.0f C: outside 0-45 C, charging is blocked", s.temp_c); return l; }
    snprintf(l.text, sizeof l.text, "ok, %.0f C", s.temp_c);
    return l;
}

inline CheckLine scPresent(bool ok, bool optional, const char* missing) {
    CheckLine l{Check::OK, "ok"};
    if (!ok) { l.level = optional ? Check::ABSENT : Check::FAIL; snprintf(l.text, sizeof l.text, "%s", missing); }
    return l;
}
