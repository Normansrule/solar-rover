// MPU-6050 IMU and the VL53L0X distance sensors: one ahead, and (optional) one on
// each front corner aimed 70 degrees out, all three behind a TCA9548A I2C mux.
// Without the mux a single front sensor on the main bus still works.
#pragma once
#include "state.h"

namespace sensors {
void begin();
void update();   // call every loop; IMU at 100 Hz, ToF when ready
}
