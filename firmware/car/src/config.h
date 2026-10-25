// Solar Rover - pin map and tuning constants.
// Every pin here matches docs/wiring.md. Change both together.
#pragma once
#include <Arduino.h>

// ---------------- I2C bus (PCA9685, MPU-6050, VL53L0X, 2x INA219) ----------------
constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;
constexpr uint8_t ADDR_PCA9685   = 0x40;
constexpr uint8_t ADDR_INA_BATT  = 0x41;   // A0 bridged
constexpr uint8_t ADDR_INA_SOLAR = 0x44;   // A1 bridged
constexpr uint8_t ADDR_MPU6050   = 0x68;
// VL53L0X is 0x29 (fixed default)

// ---------------- UART to the Raspberry Pi (agent) ----------------
constexpr int PIN_PI_RX = 16;   // ESP32 RX2  <- Pi GPIO14 (TXD, header pin 8)
constexpr int PIN_PI_TX = 17;   // ESP32 TX2  -> Pi GPIO15 (RXD, header pin 10)
constexpr uint32_t PI_BAUD = 115200;
constexpr int PIN_PI_WAKE = 15; // open-drain pulse to Pi GPIO3 (header pin 5) wakes it from halt

// ---------------- Motors: 2x TB6612FNG ----------------
// Front and rear motor on one side share the direction pins (skid steer), each
// wheel has its own PWM so traction can be balanced from the encoders.
constexpr int PIN_L_IN1   = 26;  // left driver AIN1 + BIN1
constexpr int PIN_L_IN2   = 27;  // left driver AIN2 + BIN2
constexpr int PIN_FL_PWM  = 25;  // left driver PWMA  (front-left motor)
constexpr int PIN_RL_PWM  = 33;  // left driver PWMB  (rear-left motor)
constexpr int PIN_R_IN1   = 19;  // right driver AIN1 + BIN1
constexpr int PIN_R_IN2   = 23;  // right driver AIN2 + BIN2
constexpr int PIN_FR_PWM  = 18;  // right driver PWMA (front-right motor)
constexpr int PIN_RR_PWM  = 5;   // right driver PWMB (rear-right motor)
constexpr int PIN_MOTOR_STBY = 12; // both drivers' STBY. GPIO12 is a boot strap that must be low at reset;
                                   // the TB6612s' own 200k pull-downs hold it low (and the motors off) while booting

// ---------------- Wheel encoders (LM393 slot sensors, input-only pins) ----------------
constexpr int PIN_ENC_FL = 34;
constexpr int PIN_ENC_RL = 35;
constexpr int PIN_ENC_FR = 36;   // "VP"
constexpr int PIN_ENC_RR = 39;   // "VN"

// ---------------- Power ----------------
constexpr int PIN_TEMP   = 32;   // DS18B20 probe data (1-Wire; the adapter module has the pull-up)
constexpr int PIN_CHG_EN = 4;    // relay module IN (jumper on H): HIGH = relay on = charger input connected
constexpr int PIN_LED    = 2;    // on-board LED

// ---------------- Long-range radio: Ebyte E220-900T22D LoRa module (UART) ----------------
constexpr int PIN_LORA_RX   = 13;  // ESP32 RX1 <- E220 TXD
constexpr int PIN_LORA_TX   = 14;  // ESP32 TX1 -> E220 RXD
constexpr int PIN_LORA_MODE = 0;   // E220 M0 + M1 together. High at reset (the DevKit's pull-up) = setup
                                   // mode, which is where the firmware wants it first; low = normal

// ---------------- Servo channels on the PCA9685 ----------------
enum Joint : uint8_t { J_YAW = 0, J_SHOULDER, J_ELBOW, J_WRIST, J_GRIP, J_COUNT };

struct JointCfg {
    const char* name;
    float min_deg, max_deg;   // logical limits (software end stops)
    float park_deg;           // folded/transport pose
    float offset_deg;         // servo angle = offset + dir * logical
    int8_t dir;
    float max_speed_dps;      // slew limit
};
// Logical angles: yaw 0 = forward, + = left. Shoulder 0 = horizontal forward,
// + = up. Elbow 0 = straight, + = folds down. Wrist 0 = in line, + = up.
// Gripper 0 = closed .. 100 = open (percent, mapped to servo angle).
// Calibrate offset_deg/dir with the "CAL" command (docs/assembly.md, step 9).
constexpr JointCfg JOINTS[J_COUNT] = {
    {"yaw",      -90,  90,   0,  90, +1, 90},
    {"shoulder", -35, 120, 110,  90, +1, 60},
    {"elbow",      0, 150, 120,  20, +1, 75},
    {"wrist",    -90,  90, -70,  90, +1, 120},
    {"grip",       0, 100,  20,  40, +1, 200},   // logical % -> 40..130 deg servo
};
constexpr float GRIP_SERVO_SPAN_DEG = 90;   // 0% -> offset, 100% -> offset + span
constexpr int SERVO_US_MIN = 500, SERVO_US_MAX = 2500, SERVO_FREQ_HZ = 50;

// ---------------- Drive tuning ----------------
constexpr int   MOTOR_PWM_FREQ   = 20000;   // inaudible
constexpr int   MOTOR_PWM_BITS   = 10;
constexpr float MOTOR_MAX_DUTY   = 0.80f;   // TT motors are 6V parts; pack is 7.4-8.4V
constexpr float WHEEL_DIAMETER_MM = 65.0f;
constexpr int   ENC_EDGES_PER_REV = 40;     // 20 slots, both edges
constexpr float MAX_WHEEL_MM_S   = 600.0f;  // command 1000 == this speed (closed loop)
constexpr float SPEED_KP = 0.0012f, SPEED_KI = 0.004f;
constexpr bool  CLOSED_LOOP_DEFAULT = true;
constexpr float RAMP_PER_S = 3.0f;          // max change of duty fraction per second

// ---------------- Safety / arbitration ----------------
constexpr uint32_t LINK_TIMEOUT_MS     = 300;   // no command for this long -> stop
constexpr uint32_t MANUAL_OVERRIDE_MS  = 2000;  // manual input holds control this long
constexpr uint32_t LORA_TIMEOUT_MS     = 700;   // the LoRa link sends at 10 Hz: stop after ~7 lost frames
constexpr float    LORA_SPEED_SCALE    = 0.6f;  // top speed while driving over LoRa (more lag, less view)
constexpr float BATT_LOW_PCT      = 30;   // agent switches to "seek sun / park"
constexpr float BATT_CRIT_PCT     = 15;   // everything stops, Pi told to shut down
constexpr float BATT_RESUME_PCT   = 60;   // wake the Pi again
constexpr float CHARGE_MIN_C = 0.0f, CHARGE_MAX_C = 45.0f, CHARGE_HYST_C = 3.0f;
constexpr float CHARGE_SUN_V = 9.5f;      // panel voltage that can charge a 2S pack: relay on
constexpr uint32_t WALL_PROBE_MS = 120000, WALL_PROBE_ON_MS = 1500;   // no sun: check for the wall adapter this often
constexpr float BATT_FULL_V  = 8.35f;     // stop charging at/above this (CN3795 terminates too)
constexpr float BATT_EMPTY_V = 6.6f;

// ---------------- Radio ----------------
constexpr const char* AP_PASSWORD = "solarrover123";   // change me (8+ chars)
constexpr const char* BLE_NAME    = "SolarRover";
constexpr uint16_t    WEB_PORT    = 80;

constexpr uint32_t TELEMETRY_UART_MS = 50;   // 20 Hz to the agent
constexpr uint32_t TELEMETRY_APP_MS  = 100;  // 10 Hz to phone/web
