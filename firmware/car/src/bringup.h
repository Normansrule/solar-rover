// Bring-up self-tests. TEST WHEELS runs each wheel on its own and reports
// whether it turns the right way and its encoder is on the right pin.
#pragma once
#include "state.h"

namespace bringup {
bool startWheels(Source requester);   // false if a test is already running
void abort();                         // any S command stops the test
bool running();
void update(float dt);                // call every loop
}
