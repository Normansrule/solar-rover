// PC test of the manual-driving obstacle assist. CI builds and runs it:
//   g++ -std=c++17 -Wall -Werror -I../src test_assist.cpp -o /tmp/tas && /tmp/tas
#include <cstdio>
#include <cmath>
#include "assist.h"

static int failures = 0;
static void expect(bool ok, const char* what) {
    if (!ok) { std::printf("FAIL %s\n", what); failures++; }
}
static bool near(float a, float b, float tol = 1e-4f) { return std::fabs(a - b) < tol; }

int main() {
    const float VMAX = 600;
    // nothing seen: untouched
    AssistOut o = assistLimit(1, 1, -1, -1, -1, VMAX);
    expect(!o.limiting && o.left == 1 && o.right == 1, "clear road untouched");
    // far wall: full speed still stops in time
    o = assistLimit(1, 1, 400, -1, -1, VMAX);
    expect(!o.limiting, "40 cm ahead: full speed is fine");
    // the safe speed really stops short of the margin
    for (int d = 70; d <= 600; d += 10) {
        float v = assistSafeSpeed(d);
        float stop = v * ASSIST_LAG_S + v * v / (2 * ASSIST_DECEL_MM_S2);
        expect(stop <= d - ASSIST_MARGIN_MM + 0.5f, "safe speed stops before the margin");
    }
    // closer means slower, and inside the margin means stop
    expect(assistSafeSpeed(150) < assistSafeSpeed(250), "closer is slower");
    o = assistLimit(1, 1, 50, -1, -1, VMAX);
    expect(o.limiting && near(o.left, 0) && near(o.right, 0), "inside the margin: no forward");
    // 15 cm ahead: capped, and the turn the driver asked for is kept
    o = assistLimit(0.8f, 1.0f, 150, -1, -1, VMAX);
    expect(o.limiting && near(o.right - o.left, 0.2f) && 0.5f * (o.left + o.right) < 0.9f, "capped, turn kept");
    // reverse and turning on the spot are never touched
    o = assistLimit(-1, -1, 30, 30, 30, VMAX);
    expect(!o.limiting && o.left == -1, "reverse untouched");
    o = assistLimit(-0.5f, 0.5f, 30, 30, 30, VMAX);
    expect(!o.limiting && o.left == -0.5f, "spin untouched");
    // a close left corner only matters when turning left
    o = assistLimit(1, 1, -1, 40, -1, VMAX);
    expect(!o.limiting, "straight past a close left corner");
    o = assistLimit(0.6f, 1.0f, -1, 40, -1, VMAX);
    expect(o.limiting, "turning into a close left corner slows");
    o = assistLimit(1.0f, 0.6f, -1, 40, -1, VMAX);
    expect(!o.limiting, "turning away from it does not");
    // slow driving near something is left alone
    o = assistLimit(0.2f, 0.2f, 200, -1, -1, VMAX);
    expect(!o.limiting, "slow approach untouched");

    std::printf(failures ? "%d failure(s)\n" : "assist: all checks passed\n", failures);
    return failures ? 1 : 0;
}
