// MPU-6050 IMU and VL53L0X distance sensor.
#pragma once
#include "state.h"

namespace sensors {
void begin();
void update();   // call every loop; IMU at 100 Hz, ToF when ready
}
