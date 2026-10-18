// 4WD skid-steer drive: 2x TB6612FNG, 4x TT motors, 4x slot encoders.
#pragma once
#include "state.h"

namespace drive {
void begin();
// left/right in -1..1 (fraction of full speed)
void set(float left, float right);
void stop();
void update(float dt);          // call every loop
void setClosedLoop(bool on);
void enable(bool on);           // STBY pin
}
