#include "power.h"
#include <Adafruit_INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>

namespace {
Adafruit_INA219 inaBatt(ADDR_INA_BATT);
Adafruit_INA219 inaSolar(ADDR_INA_SOLAR);
bool battOk = false, solarOk = false;
uint32_t lastMs = 0;
bool fullLatched = false;
bool tempBlocked = false;
float pctFilt = -1;

// DS18B20 pack-temperature probe, read without blocking: each read collects the
// conversion started one second earlier and starts the next one.
OneWire oneWire(PIN_TEMP);
DallasTemperature probe(&oneWire);
DeviceAddress probeAddr;
bool probeFound = false;
uint32_t lastTempMs = 0, lastSearchMs = 0;
float lastTempC = NAN;

void findProbe() {
    lastSearchMs = millis();
    probe.begin();
    probeFound = probe.getAddress(probeAddr, 0);
    if (probeFound) {
        probe.setResolution(probeAddr, 10);
        probe.setWaitForConversion(false);
        probe.requestTemperatures();
        lastTempMs = millis();
    }
}

float readTempC(bool& fault) {
    uint32_t now = millis();
    if (!probeFound) {
        if (now - lastSearchMs > 5000) findProbe();
        fault = true;
        return NAN;
    }
    if (now - lastTempMs >= 1000) {
        lastTempMs = now;
        float t = probe.getTempC(probeAddr);
        probe.requestTemperatures();
        // -127 = probe unplugged, 85 = power-on value before the first conversion
        if (t == DEVICE_DISCONNECTED_C) { probeFound = false; lastTempC = NAN; }
        else if (t != 85.0f) lastTempC = t;
    }
    fault = isnan(lastTempC);
    return lastTempC;
}

// Charger relay: on only while charging is allowed and there is input power,
// so its coil (about 70 mA at 5 V) isn't wasted at night.
bool relayOn = false;
uint32_t nextProbeMs = 0, probeEndMs = 0;

bool present(uint8_t addr) { Wire.beginTransmission(addr); return Wire.endTransmission() == 0; }
}  // namespace

namespace power {

float pctFromVoltage(float packV) {
    // resting Li-ion curve, per cell
    static const float V[] = {3.30, 3.40, 3.50, 3.60, 3.70, 3.80, 3.90, 4.00, 4.10, 4.20};
    static const float P[] = {0,    5,    10,   20,   35,   50,   65,   80,   90,   100};
    float c = packV / 2.0f;
    if (c <= V[0]) return 0;
    if (c >= V[9]) return 100;
    for (int i = 1; i < 10; i++) if (c < V[i]) return P[i-1] + (P[i]-P[i-1]) * (c - V[i-1]) / (V[i] - V[i-1]);
    return 100;
}

void begin() {
    pinMode(PIN_CHG_EN, OUTPUT);
    digitalWrite(PIN_CHG_EN, LOW);     // charger off until the temperature is known
    findProbe();
    battOk = present(ADDR_INA_BATT) && inaBatt.begin();
    solarOk = present(ADDR_INA_SOLAR) && inaSolar.begin();
    if (battOk) inaBatt.setCalibration_32V_2A();
    if (solarOk) inaSolar.setCalibration_32V_1A();
}

void update() {
    uint32_t now = millis();
    if (now - lastMs < 200) return;
    lastMs = now;

    PowerState& p = gPower;
    if (battOk) {
        p.batt_v = inaBatt.getBusVoltage_V() + inaBatt.getShuntVoltage_mV() / 1000.0f;
        p.charge_ma = inaBatt.getCurrent_mA();
    }
    if (solarOk) {
        p.solar_v = inaSolar.getBusVoltage_V();
        p.solar_mw = max(0.0f, inaSolar.getPower_mW());
    }
    bool fault;
    float t = readTempC(fault);
    p.ntc_fault = fault;
    if (!fault) p.temp_c = t;

    // State of charge: voltage based, heavily filtered; ignore while charging
    // (charging lifts the voltage) unless we have no estimate yet.
    float raw = pctFromVoltage(p.batt_v - (p.charge_ma > 50 ? 0.15f : 0.0f));
    pctFilt = (pctFilt < 0) ? raw : pctFilt + 0.02f * (raw - pctFilt);
    p.pct = pctFilt;

    // low / critical with hysteresis
    if (p.pct < BATT_LOW_PCT) p.low = true; else if (p.pct > BATT_LOW_PCT + 5) p.low = false;
    if (battOk && p.batt_v > 1.0f) {
        if (p.pct < BATT_CRIT_PCT || p.batt_v < BATT_EMPTY_V) p.critical = true;
        else if (p.pct > BATT_RESUME_PCT) p.critical = false;
    }

    // ---- charger enable ----
    if (fault) tempBlocked = true;
    else if (p.temp_c > CHARGE_MAX_C || p.temp_c < CHARGE_MIN_C) tempBlocked = true;
    else if (p.temp_c < CHARGE_MAX_C - CHARGE_HYST_C && p.temp_c > CHARGE_MIN_C + CHARGE_HYST_C) tempBlocked = false;

    if (p.batt_v >= BATT_FULL_V && p.charge_ma < 100) fullLatched = true;
    else if (p.batt_v < 8.10f) fullLatched = false;

    bool allowed = !tempBlocked && !fullLatched;
    bool charging = relayOn && p.charge_ma > 30;
    bool sun = solarOk && p.solar_v >= CHARGE_SUN_V;
    if (!allowed) { relayOn = false; probeEndMs = 0; }
    else if (probeEndMs) {                        // wall-adapter check running
        if ((int32_t)(now - probeEndMs) >= 0) { relayOn = charging; probeEndMs = 0; nextProbeMs = now + WALL_PROBE_MS; }
    }
    else if (charging || sun) relayOn = true;     // charging (solar or wall), or enough sun to start
    else if ((int32_t)(now - nextProbeMs) >= 0) { relayOn = true; probeEndMs = now + WALL_PROBE_ON_MS; }
    else relayOn = false;
    digitalWrite(PIN_CHG_EN, relayOn ? HIGH : LOW);
    p.charger_on = relayOn;

    if (fault) p.charge_state = "fault";
    else if (tempBlocked) p.charge_state = "off_temp";
    else if (fullLatched) p.charge_state = "full";
    else if (!allowed || p.charge_ma <= 30) p.charge_state = "idle";
    else {
        float chargeMw = p.charge_ma * p.batt_v;
        p.charge_state = (p.solar_mw > 0.6f * chargeMw) ? "solar" : "wall";
    }
}

}  // namespace power
