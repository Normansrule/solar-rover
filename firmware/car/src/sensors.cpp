#include "sensors.h"
#include <Wire.h>
#include <VL53L0X.h>

namespace {
// index 0 = front, 1 = left corner, 2 = right corner
VL53L0X tof[3];
bool tofOk[3] = {false, false, false};
constexpr uint8_t TOF_CH[3] = {TOF_CH_FRONT, TOF_CH_LEFT, TOF_CH_RIGHT};
bool muxPresent = false;
uint8_t muxSel = 0xFF;

bool muxSelect(int i) {
    if (!muxPresent) return true;
    uint8_t want = 1u << TOF_CH[i];
    if (muxSel == want) return true;
    Wire.beginTransmission(ADDR_TCA9548A);
    Wire.write(want);
    bool ok = Wire.endTransmission() == 0;
    muxSel = ok ? want : 0xFF;
    return ok;
}

bool tofInit(int i) {
    if (!muxSelect(i)) return false;
    tof[i].setTimeout(30);
    if (!tof[i].init()) return false;
    tof[i].setMeasurementTimingBudget(33000);
    tof[i].startContinuous(50);
    return true;
}

int* tofOut(int i) { return i == 0 ? &gSensors.tof_mm : &gSensors.side_mm[i - 1]; }
uint32_t lastImuMs = 0;
float bias[3] = {0, 0, 0};

bool imuWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ADDR_MPU6050);
    Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
}

bool imuRead(int16_t v[7]) {
    Wire.beginTransmission(ADDR_MPU6050);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)ADDR_MPU6050, 14) != 14) return false;
    for (int i = 0; i < 7; i++) v[i] = (int16_t)((Wire.read() << 8) | Wire.read());
    return true;
}
}  // namespace

namespace sensors {

void begin() {
    SensorState& s = gSensors;
    s.imu_ok = imuWrite(0x6B, 0x00)      // wake
            && imuWrite(0x1A, 0x03)      // DLPF ~44 Hz
            && imuWrite(0x1B, 0x00)      // +-250 dps
            && imuWrite(0x1C, 0x00);     // +-2 g
    if (s.imu_ok) {
        // gyro bias: keep the car still for the first second after power-on
        int16_t v[7]; float sum[3] = {0, 0, 0}; int n = 0;
        for (int i = 0; i < 100; i++) {
            if (imuRead(v)) { for (int k = 0; k < 3; k++) sum[k] += v[4 + k] / 131.0f; n++; }
            delay(5);
        }
        if (n) for (int k = 0; k < 3; k++) bias[k] = sum[k] / n;
    }
    Wire.beginTransmission(ADDR_TCA9548A);
    muxPresent = Wire.endTransmission() == 0;
    s.tof_mux = muxPresent;
    for (int i = 0; i < (muxPresent ? 3 : 1); i++) tofOk[i] = tofInit(i);
    s.tof_ok = tofOk[0];
    s.side_ok[0] = tofOk[1]; s.side_ok[1] = tofOk[2];
}

void update() {
    SensorState& s = gSensors;
    uint32_t now = millis();
    if (s.imu_ok && now - lastImuMs >= 10) {
        lastImuMs = now;
        int16_t v[7];
        if (imuRead(v)) {
            s.ax = v[0] / 16384.0f; s.ay = v[1] / 16384.0f; s.az = v[2] / 16384.0f;
            s.gx = v[4] / 131.0f - bias[0]; s.gy = v[5] / 131.0f - bias[1]; s.gz = v[6] / 131.0f - bias[2];
        }
    }
    // one sensor per loop pass, round robin, so a pass never waits on all three
    static int next = 0;
    for (int n = 0; n < 3; n++) {
        int i = next; next = (next + 1) % 3;
        if (!tofOk[i]) continue;
        if (muxSelect(i) && (tof[i].readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07)) {
            uint16_t mm = tof[i].readRangeContinuousMillimeters();
            *tofOut(i) = (tof[i].timeoutOccurred() || mm >= 2000) ? -1 : mm;
        }
        break;
    }
}

}  // namespace sensors
