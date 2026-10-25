#include "bringup.h"
#include "drive.h"
#include "links.h"
#include "wheel_check.h"

namespace {
const char* WHEEL_NAMES[4] = {"FL", "RL", "FR", "RR"};
constexpr float    TEST_DUTY = 0.55f;
constexpr uint32_t SETTLE_MS = 500, RUN_MS = 700, COAST_MS = 600;

enum class Phase { IDLE, SETTLE, RUN, COAST };
Phase phase = Phase::IDLE;
Source who = Source::NONE;
int wheel = 0;
uint32_t phaseStart = 0;
uint32_t edges0[4];
float yaw = 0;
WheelResult results[4];

void say(const String& s) { links::send(who, s); }

void enter(Phase p) { phase = p; phaseStart = millis(); }

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

void abort() {
    if (phase == Phase::IDLE) return;
    drive::testWheel(-1, 0);
    phase = Phase::IDLE;
    say("TEST aborted");
}

bool running() { return phase != Phase::IDLE; }

void update(float dt) {
    if (phase == Phase::IDLE) return;
    if (gPower.critical) { abort(); return; }
    uint32_t t = millis() - phaseStart;
    switch (phase) {
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
