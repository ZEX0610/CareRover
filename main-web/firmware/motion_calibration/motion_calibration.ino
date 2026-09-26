#include <Arduino.h>
#include <Preferences.h>

#include "continuous_servo_drive.h"
#include "motion_layout.h"

using namespace carerover;

// Chosen to avoid current CareRover allocations:
// OLED 4/5, MAX30102 8/9, CAM UART 17/18.
constexpr auto& SERVO_PINS = MOTION_SERVO_PINS;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint16_t DEFAULT_MOVE_MS = 1000;
constexpr float DEFAULT_TEST_SPEED = 0.25f;
constexpr size_t LINE_CAPACITY = 128;

const char* const WHEEL_NAMES[WHEEL_COUNT] = {"fl", "fr", "rl", "rr"};

ContinuousServoDrive drive;
Preferences preferences;
ServoCalibration calibration = {{1500, 1500, 1500, 1500}, {1, 1, 1, 1}, 300};
bool armed = false;
bool estopLatched = false;
char lineBuffer[LINE_CAPACITY];
size_t lineLength = 0;

void printError(const char* code, const char* message) {
  Serial.printf("{\"type\":\"motion_error\",\"code\":\"%s\",\"message\":\"%s\"}\n",
                code, message);
}

void printAck(const char* command, uint32_t durationMs = 0) {
  Serial.printf("{\"type\":\"motion_ack\",\"command\":\"%s\",\"duration_ms\":%lu}\n",
                command, static_cast<unsigned long>(durationMs));
}

int wheelFromName(const char* name) {
  if (!name) return -1;
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    if (strcasecmp(name, WHEEL_NAMES[i]) == 0) return i;
  }
  return -1;
}

bool parseFloatStrict(const char* text, float& value) {
  if (!text || !*text) return false;
  char* end = nullptr;
  value = strtof(text, &end);
  return end && *end == '\0' && isfinite(value);
}

bool parseLongStrict(const char* text, long& value) {
  if (!text || !*text) return false;
  char* end = nullptr;
  value = strtol(text, &end, 10);
  return end && *end == '\0';
}

void loadCalibration() {
  preferences.begin("cr-motion", true);
  if(preferences.getUInt("layout",0)!=MOTION_LAYOUT_ID){preferences.end();return;}
  calibration.speedSpanUs = preferences.getUShort("span", 300);
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    char key[8];
    snprintf(key, sizeof(key), "n%u", i);
    calibration.neutralUs[i] = preferences.getUShort(key, 1500);
    snprintf(key, sizeof(key), "d%u", i);
    calibration.directionSign[i] = preferences.getChar(key, 1) < 0 ? -1 : 1;
    snprintf(key, sizeof(key), "s%u", i);
    calibration.wheelSpanUs[i] = preferences.getUShort(key, calibration.speedSpanUs);
  }
  preferences.end();
}

void saveCalibration() {
  preferences.begin("cr-motion", false);
  preferences.putUInt("layout", MOTION_LAYOUT_ID);
  preferences.putBool("verified", false); // Every edit invalidates prior physical verification.
  preferences.putUShort("span", calibration.speedSpanUs);
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    char key[8];
    snprintf(key, sizeof(key), "n%u", i);
    preferences.putUShort(key, calibration.neutralUs[i]);
    snprintf(key, sizeof(key), "d%u", i);
    preferences.putChar(key, calibration.directionSign[i]);
    snprintf(key, sizeof(key), "s%u", i);
    preferences.putUShort(key, calibration.wheelSpanUs[i] ? calibration.wheelSpanUs[i] : calibration.speedSpanUs);
  }
  preferences.end();
  printAck("save");
}

void printStatus() {
  Serial.printf("{\"type\":\"motion_status\",\"ready\":%s,\"armed\":%s,"
                "\"estop\":%s,\"moving\":%s,\"span_us\":%u,\"wheels\":[",
                drive.ready() ? "true" : "false", armed ? "true" : "false",
                estopLatched ? "true" : "false", drive.moving() ? "true" : "false",
                calibration.speedSpanUs);
  for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
    if (i) Serial.print(',');
    Serial.printf("{\"id\":\"%s\",\"pin\":%u,\"neutral_us\":%u,\"invert\":%s}",
                  WHEEL_NAMES[i], SERVO_PINS[i], calibration.neutralUs[i],
                  calibration.directionSign[i] < 0 ? "true" : "false");
  }
  Serial.println("]}");
}

void printHelp() {
  Serial.println(F("=== CareRover X-drive standalone test ==="));
  Serial.println(F("Safety: lift wheels first; boot is DISARMED; every move auto-stops."));
  Serial.println(F("arm | disarm | stop | estop | clear_estop | status | help"));
  Serial.println(F("f|b|l|r|cw|ccw [speed_percent=25] [duration_ms=1000]"));
  Serial.println(F("mix <vx -1..1> <vy -1..1> <wz -1..1> [duration_ms]"));
  Serial.println(F("wheel <fl|fr|rl|rr> <speed_percent -100..100> [duration_ms]"));
  Serial.println(F("neutral <fl|fr|rl|rr> <1300..1700 us>   (DISARMED only)"));
  Serial.println(F("invert <fl|fr|rl|rr> <0|1>              (DISARMED only)"));
  Serial.println(F("span <100..500 us> | save | defaults    (DISARMED only)"));
  Serial.println(F("Coordinates: +vx forward, +vy right, +wz clockwise."));
}

bool requireArmed() {
  if (estopLatched) {
    printError("ESTOP_ACTIVE", "Run clear_estop, inspect the car, then arm again");
    return false;
  }
  if (!armed) {
    printError("DISARMED", "Run arm before a bounded movement test");
    return false;
  }
  return true;
}

bool requireDisarmed() {
  if (armed || drive.moving()) {
    printError("MUST_DISARM", "Run disarm before changing calibration");
    return false;
  }
  return true;
}

bool parseMoveOptions(char* speedToken, char* durationToken,
                      float& speed, uint32_t& durationMs) {
  speed = DEFAULT_TEST_SPEED;
  durationMs = DEFAULT_MOVE_MS;
  if (speedToken) {
    float percent;
    if (!parseFloatStrict(speedToken, percent) || percent < 1.0f || percent > 100.0f) {
      printError("BAD_SPEED", "speed_percent must be 1..100");
      return false;
    }
    speed = percent / 100.0f;
  }
  if (durationToken) {
    long parsed;
    if (!parseLongStrict(durationToken, parsed) || parsed < 50 ||
        parsed > static_cast<long>(ContinuousServoDrive::MAX_COMMAND_DURATION_MS)) {
      printError("BAD_DURATION", "duration_ms must be 50..5000");
      return false;
    }
    durationMs = static_cast<uint32_t>(parsed);
  }
  if (speed > 0.60f) {
    Serial.println(F("{\"type\":\"motion_warning\",\"message\":\"speed above 60%; verify low-speed floor test first\"}"));
  }
  return true;
}

void runCardinal(const char* command, char* speedToken, char* durationToken) {
  if (!requireArmed()) return;
  float speed;
  uint32_t durationMs;
  if (!parseMoveOptions(speedToken, durationToken, speed, durationMs)) return;

  float vx = 0, vy = 0, wz = 0;
  if (strcasecmp(command, "f") == 0) vx = speed;
  else if (strcasecmp(command, "b") == 0) vx = -speed;
  else if (strcasecmp(command, "l") == 0) vy = -speed;
  else if (strcasecmp(command, "r") == 0) vy = speed;
  else if (strcasecmp(command, "cw") == 0) wz = speed;
  else if (strcasecmp(command, "ccw") == 0) wz = -speed;

  if (!drive.commandChassis(vx, vy, wz, durationMs)) {
    printError("DRIVE_REJECTED", "Drive was not ready or duration was invalid");
    return;
  }
  printAck(command, durationMs);
}

void handleCommand(char* line) {
  char* context = nullptr;
  char* command = strtok_r(line, " \t", &context);
  if (!command) return;

  if (strcasecmp(command, "help") == 0 || strcmp(command, "?") == 0) {
    printHelp();
  } else if (strcasecmp(command, "status") == 0) {
    printStatus();
  } else if (strcasecmp(command, "arm") == 0) {
    if (estopLatched) {
      printError("ESTOP_ACTIVE", "Clear emergency stop first");
    } else {
      drive.stopNow();
      armed = true;
      printAck("arm");
    }
  } else if (strcasecmp(command, "disarm") == 0) {
    drive.stopNow();
    armed = false;
    printAck("disarm");
  } else if (strcasecmp(command, "stop") == 0 || strcmp(command, "!") == 0) {
    drive.stopNow();
    printAck("stop");
  } else if (strcasecmp(command, "estop") == 0) {
    drive.stopNow();
    armed = false;
    estopLatched = true;
    printAck("estop");
  } else if (strcasecmp(command, "clear_estop") == 0) {
    drive.stopNow();
    estopLatched = false;
    armed = false;
    printAck("clear_estop");
  } else if (strcasecmp(command, "f") == 0 || strcasecmp(command, "b") == 0 ||
             strcasecmp(command, "l") == 0 || strcasecmp(command, "r") == 0 ||
             strcasecmp(command, "cw") == 0 || strcasecmp(command, "ccw") == 0) {
    char* speedToken = strtok_r(nullptr, " \t", &context);
    char* durationToken = strtok_r(nullptr, " \t", &context);
    runCardinal(command, speedToken, durationToken);
  } else if (strcasecmp(command, "mix") == 0) {
    if (!requireArmed()) return;
    float vx, vy, wz;
    char* vxText = strtok_r(nullptr, " \t", &context);
    char* vyText = strtok_r(nullptr, " \t", &context);
    char* wzText = strtok_r(nullptr, " \t", &context);
    if (!parseFloatStrict(vxText, vx) || !parseFloatStrict(vyText, vy) ||
        !parseFloatStrict(wzText, wz) || fabsf(vx) > 1.0f || fabsf(vy) > 1.0f ||
        fabsf(wz) > 1.0f) {
      printError("BAD_VECTOR", "mix requires vx vy wz, each within -1..1");
      return;
    }
    uint32_t durationMs = DEFAULT_MOVE_MS;
    char* durationText = strtok_r(nullptr, " \t", &context);
    if (durationText) {
      long parsed;
      if (!parseLongStrict(durationText, parsed) || parsed < 50 || parsed > 5000) {
        printError("BAD_DURATION", "duration_ms must be 50..5000");
        return;
      }
      durationMs = static_cast<uint32_t>(parsed);
    }
    if (!drive.commandChassis(vx, vy, wz, durationMs)) {
      printError("DRIVE_REJECTED", "Drive rejected vector");
      return;
    }
    printAck("mix", durationMs);
  } else if (strcasecmp(command, "wheel") == 0) {
    if (!requireArmed()) return;
    const int wheel = wheelFromName(strtok_r(nullptr, " \t", &context));
    float percent;
    if (wheel < 0 || !parseFloatStrict(strtok_r(nullptr, " \t", &context), percent) ||
        fabsf(percent) > 100.0f) {
      printError("BAD_WHEEL", "wheel requires fl|fr|rl|rr and speed -100..100");
      return;
    }
    uint32_t durationMs = DEFAULT_MOVE_MS;
    char* durationText = strtok_r(nullptr, " \t", &context);
    if (durationText) {
      long parsed;
      if (!parseLongStrict(durationText, parsed) || parsed < 50 || parsed > 5000) {
        printError("BAD_DURATION", "duration_ms must be 50..5000");
        return;
      }
      durationMs = static_cast<uint32_t>(parsed);
    }
    WheelSpeeds speeds = {{0, 0, 0, 0}};
    speeds.value[wheel] = percent / 100.0f;
    if (!drive.commandWheels(speeds, durationMs)) {
      printError("DRIVE_REJECTED", "Drive rejected wheel test");
      return;
    }
    printAck("wheel", durationMs);
  } else if (strcasecmp(command, "neutral") == 0) {
    if (!requireDisarmed()) return;
    const int wheel = wheelFromName(strtok_r(nullptr, " \t", &context));
    long pulseUs;
    if (wheel < 0 || !parseLongStrict(strtok_r(nullptr, " \t", &context), pulseUs) ||
        pulseUs < 1300 || pulseUs > 1700) {
      printError("BAD_NEUTRAL", "neutral requires wheel and 1300..1700 us");
      return;
    }
    calibration.neutralUs[wheel] = static_cast<uint16_t>(pulseUs);
    drive.setCalibration(calibration);
    printAck("neutral");
    printStatus();
  } else if (strcasecmp(command, "invert") == 0) {
    if (!requireDisarmed()) return;
    const int wheel = wheelFromName(strtok_r(nullptr, " \t", &context));
    long invert;
    if (wheel < 0 || !parseLongStrict(strtok_r(nullptr, " \t", &context), invert) ||
        (invert != 0 && invert != 1)) {
      printError("BAD_INVERT", "invert requires wheel and 0 or 1");
      return;
    }
    calibration.directionSign[wheel] = invert ? -1 : 1;
    drive.setCalibration(calibration);
    printAck("invert");
    printStatus();
  } else if (strcasecmp(command, "span") == 0) {
    if (!requireDisarmed()) return;
    long spanUs;
    if (!parseLongStrict(strtok_r(nullptr, " \t", &context), spanUs) ||
        spanUs < 100 || spanUs > 500) {
      printError("BAD_SPAN", "span requires 100..500 us");
      return;
    }
    calibration.speedSpanUs = static_cast<uint16_t>(spanUs);
    for(int i=0;i<4;++i) calibration.wheelSpanUs[i]=spanUs;
    drive.setCalibration(calibration);
    printAck("span");
    printStatus();
  } else if (strcasecmp(command, "wheelspan") == 0) {
    if (!requireDisarmed()) return;
    const int wheel=wheelFromName(strtok_r(nullptr," \t",&context));long span;
    if(wheel<0 || !parseLongStrict(strtok_r(nullptr," \t",&context),span) || span<100 || span>500) { printError("BAD_SPAN","wheelspan fl|fr|rl|rr 100..500");return; }
    calibration.wheelSpanUs[wheel]=span;drive.setCalibration(calibration);printAck("wheelspan");
  } else if (strcasecmp(command, "confirm_calibration") == 0) {
    if (!requireDisarmed()) return;
    saveCalibration();preferences.begin("cr-motion",false);preferences.putBool("verified",true);preferences.end();
    printAck("confirm_calibration");
  } else if (strcasecmp(command, "save") == 0) {
    if (requireDisarmed()) saveCalibration();
  } else if (strcasecmp(command, "defaults") == 0) {
    if (!requireDisarmed()) return;
    for (uint8_t i = 0; i < WHEEL_COUNT; ++i) {
      calibration.neutralUs[i] = 1500;
      calibration.directionSign[i] = 1;
      calibration.wheelSpanUs[i] = 300;
    }
    calibration.speedSpanUs = 300;
    drive.setCalibration(calibration);
    printAck("defaults");
    printStatus();
  } else {
    printError("UNKNOWN_COMMAND", "Run help for the command list");
  }
}

void readSerialCommands() {
  while (Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch == '\n') {
      lineBuffer[lineLength] = '\0';
      handleCommand(lineBuffer);
      lineLength = 0;
    } else if (lineLength + 1 < LINE_CAPACITY) {
      lineBuffer[lineLength++] = ch;
    } else {
      lineLength = 0;
      printError("LINE_TOO_LONG", "Maximum command length is 127 bytes");
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);
  loadCalibration();

  const bool ok = drive.begin(SERVO_PINS, calibration);
  Serial.printf("{\"type\":\"motion_boot\",\"protocol\":1,\"layout\":\"orthogonal_4_omni\","
                "\"ready\":%s,\"baud\":%lu}\n",
                ok ? "true" : "false", static_cast<unsigned long>(SERIAL_BAUD));
  if (!ok) {
    printError("PWM_ATTACH_FAILED", "Check GPIO selection and Arduino-ESP32 core 3.x");
  }
  printHelp();
  Serial.println("wheelspan fl|fr|rl|rr 100..500 | confirm_calibration (only after physical tests)");
  printStatus();
}

void loop() {
  readSerialCommands();
  drive.tick();
  delay(1);
}
