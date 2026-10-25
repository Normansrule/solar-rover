#include "control.h"
#include "drive.h"
#include "arm.h"
#include "bringup.h"

namespace {
Mode curMode = Mode::AUTO;
Source owner = Source::NONE;
uint32_t lastSeen[6] = {0};
uint32_t lastManualMs = 0;
uint32_t lastDriveMs = 0;
bool critHandled = false;

bool isManual(Source s) { return s != Source::AGENT && s != Source::NONE; }

bool allowed(Source s) {
    if (gPower.critical) return false;
    switch (curMode) {
        case Mode::MANUAL: return isManual(s);
        case Mode::AGENT:  return s == Source::AGENT;
        case Mode::AUTO:
            if (isManual(s)) return true;
            return millis() - lastManualMs > MANUAL_OVERRIDE_MS;
    }
    return false;
}

void touch(Source s, bool activity) {
    lastSeen[(int)s] = millis();
    if (activity && isManual(s)) lastManualMs = millis();
}
}  // namespace

const char* sourceName(Source s) {
    switch (s) {
        case Source::USB: return "usb"; case Source::REMOTE: return "remote";
        case Source::WEB: return "web"; case Source::BLE: return "ble";
        case Source::AGENT: return "agent"; default: return "none";
    }
}
const char* modeName(Mode m) { return m == Mode::MANUAL ? "manual" : m == Mode::AGENT ? "agent" : "auto"; }
bool parseMode(const char* s, Mode& out) {
    if (!strcasecmp(s, "manual")) { out = Mode::MANUAL; return true; }
    if (!strcasecmp(s, "agent"))  { out = Mode::AGENT;  return true; }
    if (!strcasecmp(s, "auto"))   { out = Mode::AUTO;   return true; }
    return false;
}

namespace control {

void begin() { lastManualMs = 0; }

bool drive(Source s, float l, float r) {
    bool active = fabsf(l) > 0.02f || fabsf(r) > 0.02f;
    touch(s, active);
    if (!allowed(s)) return false;
    if (!active && owner != s && owner != Source::NONE) return true;   // idle input never steals control
    drive::set(l, r);
    owner = active ? s : (owner == s ? Source::NONE : owner);
    lastDriveMs = millis();
    return true;
}

bool arcade(Source s, float throttle, float turn) {
    float l = throttle - turn, r = throttle + turn;
    float m = max(1.0f, max(fabsf(l), fabsf(r)));
    return drive(s, l / m, r / m);
}

bool armTarget(Source s, int j, float deg) { touch(s, true); if (!allowed(s)) return false; arm::setTarget(j, deg); return true; }
bool armJog(Source s, int j, float dps) {
    bool active = fabsf(dps) > 0.5f;
    touch(s, active);
    if (!active) return true;
    if (!allowed(s)) return false;
    arm::jog(j, dps);
    return true;
}
bool armPose(Source s, const char* n) { touch(s, true); if (!allowed(s)) return false; return arm::pose(n); }
bool grip(Source s, float pct) { return armTarget(s, J_GRIP, pct); }

void stop(Source s) {
    touch(s, true);
    bringup::abort();
    drive::stop();
    arm::hold();
    owner = Source::NONE;
}

void heartbeat(Source s) { touch(s, false); }

void setMode(Source s, Mode m) {
    touch(s, false);
    if (curMode == m) return;
    curMode = m;
    drive::stop();
    arm::hold();
    owner = Source::NONE;
}

Mode mode() { return curMode; }
Source driver() { return owner; }
bool manualOverrideActive() { return curMode == Mode::AUTO && millis() - lastManualMs < MANUAL_OVERRIDE_MS && lastManualMs != 0; }

void update() {
    uint32_t now = millis();
    // link watchdog: the driving source must keep talking
    if (owner != Source::NONE && now - lastSeen[(int)owner] > LINK_TIMEOUT_MS) {
        drive::stop();
        owner = Source::NONE;
    }
    if (owner == Source::NONE && now - lastDriveMs > LINK_TIMEOUT_MS) drive::stop();
    // battery critical: stop, fold the arm, then let it go limp
    if (gPower.critical && !critHandled) {
        drive::stop();
        arm::pose("park");
        owner = Source::NONE;
        critHandled = true;
    } else if (!gPower.critical) critHandled = false;
}

}  // namespace control
