// Solar Rover - ESP32 main controller firmware.
// Real-time side of the robot: drive, arm, sensors, battery/solar charging,
// and every control link (handheld remote, phone app over WiFi/BLE, the Pi agent).
#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include "config.h"
#include "state.h"
#include "drive.h"
#include "arm.h"
#include "power.h"
#include "sensors.h"
#include "control.h"
#include "links.h"
#include "settings.h"
#include "bringup.h"

PowerState  gPower;
SensorState gSensors;
DriveState  gDrive;

namespace {
uint32_t lastUs = 0;
uint32_t critSinceMs = 0;
bool piShutdownSent = false;

void wakePi() {
    pinMode(PIN_PI_WAKE, OUTPUT);
    digitalWrite(PIN_PI_WAKE, LOW);   // GPIO3 low wakes a halted Pi
    delay(200);
    pinMode(PIN_PI_WAKE, INPUT);      // release (open drain; the Pi has its own pull-up)
}

void led() {
    // fast blink = critical, slow blink = low, solid = charging, short flash = ok
    uint32_t t = millis();
    bool on;
    if (gPower.critical) on = (t / 150) % 2;
    else if (gPower.low) on = (t / 600) % 2;
    else if (!strcmp(gPower.charge_state, "solar") || !strcmp(gPower.charge_state, "wall")) on = true;
    else on = (t % 2000) < 60;
    digitalWrite(PIN_LED, on);
}

void handleCritical() {
    if (!gPower.critical) { critSinceMs = 0; piShutdownSent = false; return; }
    if (!critSinceMs) critSinceMs = millis();
    if (!piShutdownSent) { links::sendToPi("SHUTDOWN"); piShutdownSent = true; Serial.println("Battery critical: parking, Pi shutting down"); }
    uint32_t since = millis() - critSinceMs;
    if (since > 8000) { arm::relax(true); drive::enable(false); }
    if (since > 20000) links::radiosOff();
    if (links::radiosAreOff()) {
        // Sleep in 5 s naps, waking to check the battery. The solar charger keeps
        // working on its own (the charger-enable pin holds its level in light sleep).
        Serial.flush();
        esp_sleep_enable_timer_wakeup(5ULL * 1000000ULL);
        esp_light_sleep_start();
        delay(250);
        power::update();
        if (!gPower.critical) { wakePi(); ESP.restart(); }
    }
}
}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_PI_WAKE, INPUT);
    Wire.begin(PIN_SDA, PIN_SCL, 400000);
    settings::begin();
    drive::begin();
    power::begin();
    bool armOk = arm::begin();
    for (int j = 0; j < J_COUNT; j++) { float o; int d; if (settings::loadCal(j, o, d)) arm::calibrate(j, o, d); }
    sensors::begin();
    control::begin();
    links::begin();
    drive::enable(true);
    Serial.printf("Solar Rover ready. arm=%s imu=%s tof=%s\n", armOk ? "ok" : "MISSING",
                  gSensors.imu_ok ? "ok" : "MISSING", gSensors.tof_ok ? "ok" : "MISSING");
    Serial.println("Type INFO, or see docs/protocol.md for commands.");
    lastUs = micros();
}

void loop() {
    uint32_t nowUs = micros();
    float dt = (nowUs - lastUs) / 1e6f;
    lastUs = nowUs;
    if (dt > 0.1f) dt = 0.1f;

    links::poll();
    control::update();
    bringup::update(dt);
    drive::update(dt);
    arm::update(dt);
    sensors::update();
    power::update();
    links::sendTelemetry();
    led();
    handleCritical();
    if (settings::restartRequested()) { delay(200); ESP.restart(); }
    delay(2);
}
