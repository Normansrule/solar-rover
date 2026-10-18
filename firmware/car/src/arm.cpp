#include "arm.h"
#include <Adafruit_PWMServoDriver.h>

namespace {
Adafruit_PWMServoDriver pca(ADDR_PCA9685);
bool ok = false, relaxed = false;
float cur[J_COUNT], tgt[J_COUNT], jogDps[J_COUNT];
float offs[J_COUNT];
int8_t dirs[J_COUNT];
uint32_t jogUntil[J_COUNT];

struct Pose { const char* name; float j[J_COUNT]; };
// yaw, shoulder, elbow, wrist, grip(%)
const Pose POSES[] = {
    {"park",  {0, 110, 120, -70, 20}},   // folded above the turret, clear of the camera (tools/fit_check.py)
    {"ready", {0,  45,  60, -10, 60}},
    {"pick",  {0,  10,  57, -43, 90}},   // fingertips ~3 cm off the ground, 16 cm ahead of the turret axis
    {"carry", {0,  70, 120, -40, 0}},
};

float clampJ(int j, float v) { return constrain(v, JOINTS[j].min_deg, JOINTS[j].max_deg); }

void write(int j) {
    float servoDeg = (j == J_GRIP)
        ? offs[j] + dirs[j] * (cur[j] / 100.0f) * GRIP_SERVO_SPAN_DEG
        : offs[j] + dirs[j] * cur[j];
    servoDeg = constrain(servoDeg, 0.0f, 180.0f);
    uint16_t us = SERVO_US_MIN + (uint16_t)((SERVO_US_MAX - SERVO_US_MIN) * servoDeg / 180.0f);
    pca.writeMicroseconds(j, us);
}
}  // namespace

namespace arm {

bool begin() {
    Wire.beginTransmission(ADDR_PCA9685);
    ok = Wire.endTransmission() == 0;
    for (int j = 0; j < J_COUNT; j++) {
        offs[j] = JOINTS[j].offset_deg; dirs[j] = JOINTS[j].dir;
        cur[j] = tgt[j] = JOINTS[j].park_deg; jogDps[j] = 0; jogUntil[j] = 0;
    }
    if (!ok) return false;
    pca.begin();
    pca.setOscillatorFrequency(25000000);
    pca.setPWMFreq(SERVO_FREQ_HZ);
    delay(10);
    for (int j = 0; j < J_COUNT; j++) write(j);
    return true;
}

void setTarget(int j, float deg) { if (j >= 0 && j < J_COUNT) { tgt[j] = clampJ(j, deg); jogDps[j] = 0; } }

void jog(int j, float dps) {
    if (j < 0 || j >= J_COUNT) return;
    jogDps[j] = constrain(dps, -JOINTS[j].max_speed_dps, JOINTS[j].max_speed_dps);
    jogUntil[j] = millis() + LINK_TIMEOUT_MS;
    tgt[j] = cur[j];
}

bool pose(const char* name) {
    for (const Pose& p : POSES) if (strcasecmp(p.name, name) == 0) {
        for (int j = 0; j < J_COUNT; j++) setTarget(j, p.j[j]);
        return true;
    }
    return false;
}

void hold() { for (int j = 0; j < J_COUNT; j++) { tgt[j] = cur[j]; jogDps[j] = 0; } }

void relax(bool on) {
    if (!ok || on == relaxed) return;
    relaxed = on;
    if (on) pca.sleep(); else { pca.wakeup(); for (int j = 0; j < J_COUNT; j++) write(j); }
}

float angle(int j)  { return cur[j]; }
float target(int j) { return tgt[j]; }
void calibrate(int j, float o, int d) { if (j >= 0 && j < J_COUNT) { offs[j] = o; dirs[j] = d >= 0 ? 1 : -1; } }

void update(float dt) {
    if (!ok || relaxed) return;
    uint32_t now = millis();
    // Move one big joint at a time (yaw/shoulder/elbow share the 5 A servo rail):
    // the first big joint that still has far to go gets full speed, the others wait.
    // Unfold the elbow before lowering the shoulder, and raise the shoulder before
    // folding the elbow, so the hand never swings into the turret or the nose
    // (tools/fit_check.py --paths checks every move between the poses).
    const int order[3] = {J_YAW, tgt[J_ELBOW] < cur[J_ELBOW] ? J_ELBOW : J_SHOULDER,
                                 tgt[J_ELBOW] < cur[J_ELBOW] ? J_SHOULDER : J_ELBOW};
    int leader = -1;
    for (int j : order)
        if (jogDps[j] == 0 && fabsf(tgt[j] - cur[j]) > 1.0f) { leader = j; break; }
    for (int j = 0; j < J_COUNT; j++) {
        float before = cur[j];
        if (jogDps[j] != 0) {
            if ((int32_t)(now - jogUntil[j]) > 0) { jogDps[j] = 0; tgt[j] = cur[j]; }
            else { cur[j] = clampJ(j, cur[j] + jogDps[j] * dt); tgt[j] = cur[j]; }
        } else {
            bool big = j <= J_ELBOW;
            if (big && leader != -1 && j != leader) continue;
            float step = JOINTS[j].max_speed_dps * dt;
            float d = tgt[j] - cur[j];
            cur[j] += constrain(d, -step, step);
        }
        if (fabsf(cur[j] - before) > 0.05f) write(j);
    }
}

}  // namespace arm
