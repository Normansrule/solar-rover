// PC test of the wheel-test judge. CI builds and runs it:
//   g++ -std=c++17 -Wall -Werror -I../src test_wheel_check.cpp -o /tmp/twc && /tmp/twc
#include <cstdio>
#include "wheel_check.h"

static int failures = 0;
static void expect(bool ok, const char* what) {
    if (!ok) { std::printf("FAIL %s\n", what); failures++; }
}

int main() {
    // FL forward on the floor: its own encoder counts and the car turns right.
    WheelRun fl{{30, 0, 1, 0}, -22.0f};
    expect(judgeWheel(0, fl, true).verdict == WheelVerdict::OK, "FL ok");
    // FR forward turns the car left.
    WheelRun fr{{0, 0, 28, 0}, 25.0f};
    expect(judgeWheel(2, fr, true).verdict == WheelVerdict::OK, "FR ok");
    // RL wired backwards: the car turns left instead of right.
    WheelRun rl{{0, 31, 0, 0}, 19.0f};
    expect(judgeWheel(1, rl, true).verdict == WheelVerdict::REVERSED, "RL reversed");
    // RR moved, but the FR input counted it.
    WheelRun rr{{0, 0, 27, 0}, 20.0f};
    WheelResult r = judgeWheel(3, rr, true);
    expect(r.verdict == WheelVerdict::ENC_SWAPPED && r.counted_by == 2, "RR encoder swapped with FR");
    // The car turned but no encoder counted.
    WheelRun silent{{0, 2, 0, 0}, -18.0f};
    expect(judgeWheel(0, silent, true).verdict == WheelVerdict::ENC_MISSING, "encoder missing");
    // Nothing happened at all.
    WheelRun dead{{0, 0, 0, 0}, 0.4f};
    expect(judgeWheel(0, dead, true).verdict == WheelVerdict::NO_MOTION, "no motion");
    // Car on a stand: the wheel spins but the car can't turn.
    WheelRun lifted{{40, 0, 0, 0}, 0.5f};
    expect(judgeWheel(0, lifted, true).verdict == WheelVerdict::DIR_UNKNOWN, "lifted");
    // No IMU: never claim a direction.
    expect(judgeWheel(0, fl, false).verdict == WheelVerdict::DIR_UNKNOWN, "no imu");
    // All four reversed points at an upside-down IMU.
    WheelResult all[4];
    WheelRun runs[4] = {{{30, 0, 0, 0}, 20}, {{0, 30, 0, 0}, 20}, {{0, 0, 30, 0}, -20}, {{0, 0, 0, 30}, -20}};
    for (int i = 0; i < 4; i++) all[i] = judgeWheel(i, runs[i], true);
    expect(imuLooksFlipped(all), "imu flipped");
    all[0] = judgeWheel(0, fl, true);
    expect(!imuLooksFlipped(all), "imu fine when one wheel is ok");

    std::printf(failures ? "%d failed\n" : "all wheel-check tests passed\n", failures);
    return failures ? 1 : 0;
}
