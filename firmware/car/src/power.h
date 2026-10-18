// Battery, solar and charger control (2x INA219, DS18B20 pack probe, charger relay).
#pragma once
#include "state.h"

namespace power {
void begin();
void update();          // call every loop; samples at 5 Hz
float pctFromVoltage(float packV);
}
