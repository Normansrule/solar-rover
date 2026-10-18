// Who is in control: manual (remote, phone, USB) vs. the agent, plus the
// link watchdog and the low-battery override.
#pragma once
#include "state.h"

namespace control {
void begin();
void update();                                   // call every loop

// Each returns false when the command was refused (wrong mode / battery critical).
bool drive(Source s, float left, float right);   // -1..1 tank
bool arcade(Source s, float throttle, float turn);
bool armTarget(Source s, int joint, float deg);
bool armJog(Source s, int joint, float dps);
bool armPose(Source s, const char* name);
bool grip(Source s, float pct);
void stop(Source s);                              // always accepted
void heartbeat(Source s);
void setMode(Source s, Mode m);

Mode mode();
Source driver();          // source currently driving (NONE when idle)
bool manualOverrideActive();
}
