// PC test of the bring-up sensor judge. CI builds and runs it:
//   g++ -std=c++17 -Wall -Werror -I../src test_sensor_check.cpp -o /tmp/tsc && /tmp/tsc
#include <cstdio>
#include <cstring>
#include "sensor_check.h"

static int failures = 0;
static void expect(bool ok, const char* what) {
    if (!ok) { std::printf("FAIL %s\n", what); failures++; }
}
static bool has(const CheckLine& l, const char* s) { return std::strstr(l.text, s) != nullptr; }

static SensorSummary good() {
    SensorSummary s{};
    s.imu_ok = true; s.ax = 0.02f; s.ay = -0.01f; s.az = 0.99f; s.gyro_abs = 0.4f;
    for (int i = 0; i < 3; i++) { s.tof_ok[i] = true; s.tof_samples[i] = 40; s.tof_valid[i] = 40; s.tof_min[i] = 600; s.tof_max[i] = 640; }
    s.mux = true; s.batt_ok = s.solar_ok = true; s.batt_v = 7.9f; s.solar_v = 0.1f;
    s.temp_fault = false; s.temp_c = 22; s.pca_ok = s.lora_ok = true;
    return s;
}

int main() {
    SensorSummary s = good();
    expect(scImu(s).level == Check::OK, "good IMU");
    for (int i = 0; i < 3; i++) expect(scTof(s, i).level == Check::OK && has(scTof(s, i), "62 cm"), "good ToF shows distance");
    expect(scBattery(s).level == Check::OK, "good battery");
    expect(scSolar(s).level == Check::OK && has(scSolar(s), "dark"), "dark panels are fine");
    expect(scTemp(s).level == Check::OK, "good temperature");

    s = good(); s.az = -0.98f;
    expect(scImu(s).level == Check::FAIL && has(scImu(s), "upside down"), "upside-down IMU");
    s = good(); s.ax = 0.34f; s.az = 0.94f;
    expect(scImu(s).level == Check::WARN && has(scImu(s), "tilted"), "tilted IMU");
    s = good(); s.gyro_abs = 9;
    expect(scImu(s).level == Check::WARN, "gyro moving while still");
    s = good(); s.imu_ok = false;
    expect(scImu(s).level == Check::FAIL, "missing IMU");

    s = good(); s.tof_valid[0] = 0;
    expect(scTof(s, 0).level == Check::OK && has(scTof(s, 0), "nothing"), "clear ahead is ok");
    s = good(); s.tof_min[1] = 12; s.tof_max[1] = 18;
    expect(scTof(s, 1).level == Check::WARN && has(scTof(s, 1), "film"), "covered sensor");
    s = good(); s.mux = false; s.tof_ok[1] = s.tof_ok[2] = false;
    expect(scTof(s, 1).level == Check::ABSENT && scTof(s, 2).level == Check::ABSENT, "no mux: corners not fitted");
    s = good(); s.tof_ok[2] = false;
    expect(scTof(s, 2).level == Check::FAIL && has(scTof(s, 2), "channel 2"), "mux but right corner missing");
    s = good(); s.tof_samples[0] = 0;
    expect(scTof(s, 0).level == Check::FAIL, "answered but silent");

    s = good(); s.batt_v = 3.9f;
    expect(scBattery(s).level == Check::FAIL, "one cell or flat");
    s = good(); s.batt_v = 12.4f;
    expect(scBattery(s).level == Check::FAIL && has(scBattery(s), "too high"), "3S pack");
    s = good(); s.batt_v = 6.5f;
    expect(scBattery(s).level == Check::WARN, "nearly empty");
    s = good(); s.batt_ok = false;
    expect(scBattery(s).level == Check::FAIL && has(scBattery(s), "A0"), "battery INA missing");
    s = good(); s.temp_fault = true;
    expect(scTemp(s).level == Check::FAIL, "probe missing");
    s = good(); s.temp_c = 51;
    expect(scTemp(s).level == Check::WARN, "too hot to charge");
    expect(scPresent(false, true, "x").level == Check::ABSENT, "optional part");
    expect(scPresent(false, false, "x").level == Check::FAIL, "required part");

    std::printf(failures ? "%d failure(s)\n" : "sensor check: all checks passed\n", failures);
    return failures ? 1 : 0;
}
