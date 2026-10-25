#include "protocol.h"
#include "control.h"
#include "drive.h"
#include "arm.h"
#include "settings.h"
#include "longrange.h"
#include "../../common/lora_link.h"

namespace {
const char* JOINT_KEYS[J_COUNT] = {"yaw", "shoulder", "elbow", "wrist", "grip"};

int jointIndex(const char* s) {
    if (isdigit((unsigned char)s[0])) { int j = atoi(s); return (j >= 0 && j < J_COUNT) ? j : -1; }
    for (int j = 0; j < J_COUNT; j++) if (!strcasecmp(s, JOINT_KEYS[j])) return j;
    return -1;
}

float num(const char* s) { return (float)atof(s); }

// Adafruit Bluefruit Connect "Control Pad": "!B<btn><0|1><crc>". Arrows drive,
// 1 = gripper open/close, 2 = ready pose, 3 = park pose, 4 = stop.
bool bluefruit(Source src, const char* line) {
    if (line[0] != '!' || line[1] != 'B' || strlen(line) < 4) return false;
    int btn = line[2] - '0';
    bool down = line[3] == '1';
    static bool held[9] = {false};
    static bool gripOpen = false;
    if (btn < 1 || btn > 8) return true;
    held[btn] = down;
    if (down && btn == 1) { gripOpen = !gripOpen; control::grip(src, gripOpen ? 90 : 0); }
    if (down && btn == 2) control::armPose(src, "ready");
    if (down && btn == 3) control::armPose(src, "park");
    if (down && btn == 4) control::stop(src);
    float thr = (held[5] ? 0.6f : 0) - (held[6] ? 0.6f : 0);
    float turn = (held[7] ? 0.5f : 0) - (held[8] ? 0.5f : 0);
    control::arcade(src, thr, turn);
    return true;
}
}  // namespace

namespace protocol {

bool handle(Source src, const char* lineIn, String& reply) {
    reply = "";
    char line[160];
    strlcpy(line, lineIn, sizeof(line));
    // trim
    size_t n = strlen(line);
    while (n && (line[n-1] == '\r' || line[n-1] == '\n' || line[n-1] == ' ')) line[--n] = 0;
    if (!n) return true;
    if (bluefruit(src, line)) return true;

    char* argv[8]; int argc = 0;
    char* save = nullptr;
    for (char* t = strtok_r(line, " ", &save); t && argc < 8; t = strtok_r(nullptr, " ", &save)) argv[argc++] = t;
    const char* c = argv[0];

    if (!strcasecmp(c, "D") && argc >= 3) {               // D <left> <right>   (-1000..1000)
        if (!control::drive(src, num(argv[1]) / 1000.0f, num(argv[2]) / 1000.0f)) reply = "ERR mode";
    } else if (!strcasecmp(c, "V") && argc >= 3) {        // V <throttle> <turn> (-1000..1000)
        if (!control::arcade(src, num(argv[1]) / 1000.0f, num(argv[2]) / 1000.0f)) reply = "ERR mode";
    } else if (!strcasecmp(c, "A") && argc >= 2) {        // A <yaw> <shoulder> <elbow> <wrist> <grip>  ("*" = keep)
        for (int j = 0; j < J_COUNT && j + 1 < argc; j++)
            if (strcmp(argv[j + 1], "*") != 0 && !control::armTarget(src, j, num(argv[j + 1]))) { reply = "ERR mode"; break; }
    } else if (!strcasecmp(c, "J") && argc >= 3) {        // J <joint> <deg/s>
        int j = jointIndex(argv[1]);
        if (j < 0) reply = "ERR joint"; else if (!control::armJog(src, j, num(argv[2]))) reply = "ERR mode";
    } else if (!strcasecmp(c, "G") && argc >= 2) {        // G <0..100>
        if (!control::grip(src, num(argv[1]))) reply = "ERR mode";
    } else if (!strcasecmp(c, "P") && argc >= 2) {        // P <park|ready|pick|carry>
        if (!control::armPose(src, argv[1])) reply = "ERR pose";
    } else if (!strcasecmp(c, "M") && argc >= 2) {        // M <manual|auto|agent>
        Mode m; if (parseMode(argv[1], m)) control::setMode(src, m); else reply = "ERR mode name";
    } else if (!strcasecmp(c, "S")) {
        control::stop(src);
    } else if (!strcasecmp(c, "H")) {
        control::heartbeat(src);
    } else if (!strcasecmp(c, "CL") && argc >= 2) {       // CL <0|1> closed-loop wheel speed
        drive::setClosedLoop(atoi(argv[1]) != 0);
    } else if (!strcasecmp(c, "LORA")) {                  // LORA | LORA REGION <US|EU> | LORA REKEY
        lora::Region r;
        if (src == Source::REMOTE || src == Source::LORA || src == Source::AGENT) reply = "ERR not from this link";
        else if (argc == 1) {
            uint32_t ago = longrange::lastFrameMs() ? (millis() - longrange::lastFrameMs()) / 1000 : 0;
            reply = String("LORA ") + (longrange::ok() ? "ok " : "missing ") + longrange::regionName() +
                    " rssi=" + longrange::lastRssi() + " last=" + (longrange::lastFrameMs() ? String(ago) + "s" : String("never"));
        } else if (argc >= 3 && !strcasecmp(argv[1], "REGION") && lora::parseRegion(argv[2], r)) {
            longrange::setRegion((uint8_t)r); settings::startPairing();
            reply = String("OK ") + longrange::regionName() + ": pairing open 60 s, power the remote on near the car";
        } else if (argc >= 2 && !strcasecmp(argv[1], "REKEY")) {
            longrange::rekey(); settings::startPairing();
            reply = "OK new key: pairing open 60 s, power the remote on near the car";
        } else reply = "ERR usage: LORA | LORA REGION <US|EU> | LORA REKEY";
    } else if (!strcasecmp(c, "CAL") && argc >= 4) {      // CAL <joint> <offset_deg> <dir>
        int j = jointIndex(argv[1]);
        if (j < 0) reply = "ERR joint"; else { arm::calibrate(j, num(argv[2]), atoi(argv[3])); settings::saveCal(j, num(argv[2]), atoi(argv[3])); reply = "OK"; }
    } else if (!strcasecmp(c, "WIFI")) {                  // WIFI <ssid> <password> | WIFI AP
        if (src == Source::REMOTE) { reply = "ERR"; }
        else if (argc >= 2 && !strcasecmp(argv[1], "AP")) { settings::saveWifi("", ""); reply = "OK restart to apply"; }
        else if (argc >= 3) { settings::saveWifi(argv[1], argv[2]); reply = "OK restart to apply"; }
        else reply = "ERR usage: WIFI <ssid> <password>";
    } else if (!strcasecmp(c, "PAIR")) {                  // accept a new handheld remote for 60 s
        settings::startPairing(); reply = "OK pairing 60s";
    } else if (!strcasecmp(c, "RESTART")) {
        reply = "OK"; settings::requestRestart();
    } else if (!strcasecmp(c, "INFO")) {
        reply = String("solar-rover fw 0.1 mode=") + modeName(control::mode()) + " ip=" + settings::ipString();
    } else {
        reply = "ERR unknown";
        return false;
    }
    return true;
}

size_t telemetry(char* b, size_t len) {
    const PowerState& p = gPower; const SensorState& s = gSensors; const DriveState& d = gDrive;
    int w = snprintf(b, len,
        "{\"t\":%lu,\"mode\":\"%s\",\"src\":\"%s\",\"ovr\":%d,"
        "\"bat_v\":%.2f,\"bat_pct\":%.0f,\"chg_ma\":%.0f,\"sol_v\":%.2f,\"sol_mw\":%.0f,"
        "\"temp_c\":%.1f,\"chg\":\"%s\",\"low\":%d,\"crit\":%d,"
        "\"enc\":[%ld,%ld,%ld,%ld],\"spd\":[%.0f,%.0f,%.0f,%.0f],"
        "\"imu\":[%.3f,%.3f,%.3f,%.1f,%.1f,%.1f],\"tof\":%d,"
        "\"arm\":[%.1f,%.1f,%.1f,%.1f,%.0f],\"lr\":[%d,%d,%lu]}",
        (unsigned long)millis(), modeName(control::mode()), sourceName(control::driver()), control::manualOverrideActive(),
        p.batt_v, p.pct, p.charge_ma, p.solar_v, p.solar_mw, p.temp_c, p.charge_state, p.low, p.critical,
        d.enc[0], d.enc[1], d.enc[2], d.enc[3], d.speed_mm_s[0], d.speed_mm_s[1], d.speed_mm_s[2], d.speed_mm_s[3],
        s.ax, s.ay, s.az, s.gx, s.gy, s.gz, s.tof_mm,
        arm::angle(0), arm::angle(1), arm::angle(2), arm::angle(3), arm::angle(4),
        longrange::ok(), longrange::lastRssi(),
        (unsigned long)(longrange::lastFrameMs() ? min<uint32_t>(millis() - longrange::lastFrameMs(), 999999) : 999999));
    return w < 0 ? 0 : min((size_t)w, len - 1);
}

}  // namespace protocol
