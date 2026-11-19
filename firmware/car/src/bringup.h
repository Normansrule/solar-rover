// Bring-up self-tests. TEST WHEELS runs each wheel on its own and reports
// whether it turns the right way and its encoder is on the right pin.
// TEST SENSORS watches every sensor for 2 s with the car still and reports
// one line per sensor (sensor_check.h).
#pragma once
#include "state.h"

namespace bringup {
bool startWheels(Source requester);   // false if a test is already running
bool startSensors(Source requester);  // false if a test is already running
void abort();                         // any S command stops the test
bool running();
void update(float dt);                // call every loop
}
