#include "bringup.h"
#include "drive.h"
#include "links.h"
#include "wheel_check.h"
#include "sensor_check.h"
#include "arm.h"
#include "longrange.h"

namespace {
const char* WHEEL_NAMES[4] = {"FL", "RL", "FR", "RR"};
constexpr float    TEST_DUTY = 0.55f;
constexpr uint32_t SETTLE_MS = 500, RUN_MS = 700, COAST_MS = 600;

enum class Phase { IDLE, SETTLE, RUN, COAST, SENSE };
constexpr uint32_t SENSE_MS = 2000, SENSE_EVERY_MS = 50;
Phase phase = Phase::IDLE;
Source who = Source::NONE;
int wheel = 0;
uint32_t phaseStart = 0;
uint32_t edges0[4];
float yaw = 0;
WheelResult results[4];

void say(const String& s) { links::send(who, s); }

void enter(Phase p) { phase = p; phaseStart = millis(); }

// TEST SENSORS accumulators
SensorSummary sum;
int imuN = 0;
uint32_t lastSense = 0;

void senseSample() {
    const SensorState& z = gSensors;
    sum.ax += z.ax; sum.ay += z.ay; sum.az += z.az;
    sum.gyro_abs += fabsf(z.gx) + fabsf(z.gy) + fabsf(z.gz);
    imuN++;
    const int mm[3] = {z.tof_mm, z.side_mm[0], z.side_mm[1]};
    for (int i = 0; i < 3; i++) {
        if (!sum.tof_ok[i]) continue;
        sum.tof_samples[i]++;
        if (mm[i] < 0) continue;
        if (sum.tof_valid[i] == 0 || mm[i] < sum.tof_min[i]) sum.tof_min[i] = mm[i];
        if (sum.tof_valid[i] == 0 || mm[i] > sum.tof_max[i]) sum.tof_max[i] = mm[i];
        sum.tof_valid[i]++;
    }
}

void report(const char* name, const CheckLine& l, int& problems) {
    static const char* tag[4] = {"ok", "WARN", "FAIL", "--"};
    if (l.level == Check::WARN || l.level == Check::FAIL) problems++;
    say(String("TEST ") + name + " " + (l.level == Check::OK ? "" : String(tag[(int)l.level]) + " ") + l.text);
}

void finishSensors() {
    if (imuN) { sum.ax /= imuN; sum.ay /= imuN; sum.az /= imuN; sum.gyro_abs /= imuN; }
    const PowerState& p = gPower;
    sum.batt_ok = p.batt_ok; sum.solar_ok = p.solar_ok; sum.batt_v = p.batt_v; sum.solar_v = p.solar_v;
    sum.temp_fault = p.ntc_fault; sum.temp_c = p.temp_c;
    sum.pca_ok = arm::found(); sum.lora_ok = longrange::ok();
    int problems = 0;
    report("imu", scImu(sum), problems);
    report("tof front", scTof(sum, 0), problems);
    report("tof left", scTof(sum, 1), problems);
    report("tof right", scTof(sum, 2), problems);
    report("battery", scBattery(sum), problems);
    report("solar", scSolar(sum), problems);
    report("temp", scTemp(sum), problems);
    report("servo board", scPresent(sum.pca_ok, false, "PCA9685 0x40 missing: check its hub lead and VCC"), problems);
    report("lora", scPresent(sum.lora_ok, true, "E220 not found: fine if you didn't fit one"), problems);
    say(problems ? String("TEST done: ") + problems + " to fix" : String("TEST done: all sensors ok"));
}

void finishWheel() {
    WheelRun run;
    for (int i = 0; i < 4; i++) run.edges[i] = (long)(drive::rawEdges(i) - edges0[i]);
    run.yaw_deg = yaw;
    WheelResult r = judgeWheel(wheel, run, gSensors.imu_ok);
    results[wheel] = r;
    String line = String("TEST wheel ") + WHEEL_NAMES[wheel] + " " + wheelVerdictText(r.verdict);
    if (r.verdict == WheelVerdict::ENC_SWAPPED)
        line += String(": counted on the ") + WHEEL_NAMES[r.counted_by] + " encoder input, swap those two encoder jumpers";
    line += String(" (edges ") + run.edges[0] + "/" + run.edges[1] + "/" + run.edges[2] + "/" + run.edges[3] +
            ", yaw " + String(yaw, 1) + " deg)";
    say(line);
}

void finishAll() {
    int ok = 0;
    for (auto& r : results) ok += r.verdict == WheelVerdict::OK;
    if (imuLooksFlipped(results))
        say("TEST hint: all four read reversed. Watch one wheel: if it rolls forward, the MPU-6050 is upside down (chip side must face up)");
    say(String("TEST done ") + ok + "/4 ok");
}
}  // namespace

namespace bringup {

bool startWheels(Source requester) {
    if (phase != Phase::IDLE) return false;
    who = requester;
    wheel = 0;
    drive::stop();
    say("TEST wheels: car on the floor with 50 cm free all round. Send S to stop.");
    enter(Phase::SETTLE);
    return true;
}

bool startSensors(Source requester) {
    if (phase != Phase::IDLE) return false;
    who = requester;
    drive::stop();
    sum = SensorSummary{};
    const SensorState& z = gSensors;
    sum.imu_ok = z.imu_ok; sum.mux = z.tof_mux;
    sum.tof_ok[0] = z.tof_ok; sum.tof_ok[1] = z.side_ok[0]; sum.tof_ok[2] = z.side_ok[1];
    imuN = 0; lastSense = 0;
    say("TEST sensors: keep the car still on level ground for 2 s.");
    enter(Phase::SENSE);
    return true;
}

void abort() {
    if (phase == Phase::IDLE) return;
    drive::testWheel(-1, 0);
    phase = Phase::IDLE;
    say("TEST aborted");
}

bool running() { return phase != Phase::IDLE; }

void update(float dt) {
    if (phase == Phase::IDLE) return;
    if (gPower.critical && phase != Phase::SENSE) { abort(); return; }   // the sensor check doesn't move anything
    uint32_t t = millis() - phaseStart;
    switch (phase) {
        case Phase::SENSE:
            if (millis() - lastSense >= SENSE_EVERY_MS) { lastSense = millis(); senseSample(); }
            if (t >= SENSE_MS) { phase = Phase::IDLE; finishSensors(); }
            break;
        case Phase::SETTLE:
            drive::testWheel(wheel, 0);
            if (t >= SETTLE_MS) {
                for (int i = 0; i < 4; i++) edges0[i] = drive::rawEdges(i);
                yaw = 0;
                drive::testWheel(wheel, TEST_DUTY);
                enter(Phase::RUN);
            }
            break;
        case Phase::RUN:
        case Phase::COAST:
            yaw += gSensors.gz * dt;   // keep counting while it coasts to a stop
            if (phase == Phase::RUN && t >= RUN_MS) { drive::testWheel(wheel, 0); enter(Phase::COAST); }
            else if (phase == Phase::COAST && t >= COAST_MS) {
                finishWheel();
                if (++wheel < 4) enter(Phase::SETTLE);
                else { drive::testWheel(-1, 0); phase = Phase::IDLE; finishAll(); }
            }
            break;
        default: break;
    }
}

}  // namespace bringup
