#include "drive.h"

namespace {
constexpr int PWM_PINS[4] = {PIN_FL_PWM, PIN_RL_PWM, PIN_FR_PWM, PIN_RR_PWM};
constexpr int ENC_PINS[4] = {PIN_ENC_FL, PIN_ENC_RL, PIN_ENC_FR, PIN_ENC_RR};
constexpr float MM_PER_EDGE = PI * WHEEL_DIAMETER_MM / ENC_EDGES_PER_REV;
constexpr int PWM_MAX = (1 << MOTOR_PWM_BITS) - 1;

volatile uint32_t encRaw[4] = {0, 0, 0, 0};
volatile uint32_t encLastUs[4] = {0, 0, 0, 0};
uint32_t encPrev[4] = {0, 0, 0, 0};
float integ[4] = {0, 0, 0, 0};
float target[2] = {0, 0};   // left, right (-1..1)
int   dirSign[2] = {0, 0};  // direction currently applied to each side
bool  closedLoop = CLOSED_LOOP_DEFAULT;
uint32_t lastSpeedMs = 0;
int   testIdx = -1;      // wheel under bring-up test, -1 = normal driving
float testDuty = 0;

template <int I> void IRAM_ATTR encIsr() {
    uint32_t now = micros();
    if (now - encLastUs[I] > 800) {   // ignore edges closer than 0.8 ms (noise)
        encRaw[I]++;
        encLastUs[I] = now;
    }
}

void applyDirection(int side, int sign) {
    int in1 = side == 0 ? PIN_L_IN1 : PIN_R_IN1;
    int in2 = side == 0 ? PIN_L_IN2 : PIN_R_IN2;
    // Right-side motors are mounted mirrored, so "forward" is reversed there.
    int s = side == 0 ? sign : -sign;
    digitalWrite(in1, s > 0 ? HIGH : LOW);
    digitalWrite(in2, s < 0 ? HIGH : LOW);
    dirSign[side] = sign;
}
}  // namespace

namespace drive {

void begin() {
    pinMode(PIN_MOTOR_STBY, OUTPUT);
    digitalWrite(PIN_MOTOR_STBY, LOW);
    for (int p : {PIN_L_IN1, PIN_L_IN2, PIN_R_IN1, PIN_R_IN2}) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
    for (int i = 0; i < 4; i++) {
        ledcSetup(i, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
        ledcAttachPin(PWM_PINS[i], i);
        ledcWrite(i, 0);
        pinMode(ENC_PINS[i], INPUT);   // GPIO34-39: no internal pull-ups; the LM393 board has one
    }
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_FL), encIsr<0>, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_RL), encIsr<1>, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_FR), encIsr<2>, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_RR), encIsr<3>, CHANGE);
    lastSpeedMs = millis();
}

void enable(bool on) { digitalWrite(PIN_MOTOR_STBY, on ? HIGH : LOW); }

void set(float left, float right) {
    target[0] = constrain(left, -1.0f, 1.0f);
    target[1] = constrain(right, -1.0f, 1.0f);
    gDrive.cmd_left = target[0];
    gDrive.cmd_right = target[1];
}

void stop() { set(0, 0); }

void testWheel(int wheel, float duty) { testIdx = wheel; testDuty = constrain(duty, -1.0f, 1.0f); for (float& v : integ) v = 0; }

uint32_t rawEdges(int wheel) { return encRaw[wheel]; }

void setClosedLoop(bool on) { closedLoop = on; for (float& v : integ) v = 0; }

void update(float dt) {
    // --- speed estimate every 50 ms ---
    uint32_t now = millis();
    if (now - lastSpeedMs >= 50) {
        float sec = (now - lastSpeedMs) / 1000.0f;
        lastSpeedMs = now;
        for (int i = 0; i < 4; i++) {
            uint32_t raw = encRaw[i];
            uint32_t d = raw - encPrev[i];
            encPrev[i] = raw;
            int side = i < 2 ? 0 : 1;
            int sign = dirSign[side] == 0 ? 1 : dirSign[side];   // single-channel encoder: sign from direction
            gDrive.enc[i] += sign * (long)d;
            float v = sign * d * MM_PER_EDGE / sec;
            gDrive.speed_mm_s[i] = 0.6f * gDrive.speed_mm_s[i] + 0.4f * v;
        }
    }

    // --- per-wheel duty ---
    for (int i = 0; i < 4; i++) {
        int side = i < 2 ? 0 : 1;
        float cmd = target[side];
        float want = cmd;   // open loop: duty fraction == command
        if (testIdx >= 0) {
            want = i == testIdx ? testDuty : 0;
            integ[i] = 0;
        } else if (closedLoop && fabsf(cmd) > 0.02f) {
            float err = cmd * MAX_WHEEL_MM_S - gDrive.speed_mm_s[i];
            integ[i] = constrain(integ[i] + err * SPEED_KI * dt, -0.4f, 0.4f);
            want = cmd + err * SPEED_KP + integ[i];
            if (want * cmd < 0) want = 0;   // never fight the commanded direction
        } else {
            integ[i] = 0;
        }
        want = constrain(want, -1.0f, 1.0f);
        // ramp
        float step = RAMP_PER_S * dt;
        float& d = gDrive.duty[i];
        if (want > d + step) d += step; else if (want < d - step) d -= step; else d = want;
    }

    // --- apply: direction per side, PWM per wheel ---
    for (int side = 0; side < 2; side++) {
        float a = gDrive.duty[side * 2], b = gDrive.duty[side * 2 + 1];
        float m = fabsf(a) > fabsf(b) ? a : b;
        int sign = m > 0.01f ? 1 : (m < -0.01f ? -1 : 0);
        if (sign != dirSign[side]) applyDirection(side, sign);
    }
    for (int i = 0; i < 4; i++) {
        int side = i < 2 ? 0 : 1;
        float d = gDrive.duty[i];
        float mag = (dirSign[side] == 0 || d * dirSign[side] <= 0) ? 0 : fabsf(d);
        ledcWrite(i, (uint32_t)(mag * MOTOR_MAX_DUTY * PWM_MAX));
    }
}

}  // namespace drive
