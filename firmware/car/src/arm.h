// 4-DOF arm + gripper on a PCA9685 (channels 0-4).
#pragma once
#include "state.h"

namespace arm {
bool begin();                         // false if the PCA9685 is not found
void setTarget(int joint, float deg); // logical degrees (grip: 0-100 %)
void jog(int joint, float dps);       // run a joint at dps until the next jog/timeout
bool pose(const char* name);          // park | ready | pick | carry
void hold();                          // stop all motion where it is
void update(float dt);                // call every loop (does the I2C writes)
void relax(bool on);                  // true: stop servo pulses (low power, arm goes limp)
float angle(int joint);
float target(int joint);
void calibrate(int joint, float offset_deg, int dir);   // runtime calibration (not saved)
}
