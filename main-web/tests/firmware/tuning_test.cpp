#include "health_quality.h"
#include "gesture_actions.h"
#include "imu_filter.h"
#include "person_follow.h"
#include "ppg_rate_estimator.h"
#include "safety_controller.h"
#include "signal_state_filters.h"
#include "box_track.h"

#include <cmath>
#include <iostream>

using namespace carerover;

namespace {
int failures = 0;

void expect(bool condition, const char* name) {
  if (!condition) {
    std::cerr << "FAIL: " << name << '\n';
    ++failures;
  }
}

VisionPacket face(uint32_t seq, uint64_t now, int x, int width, uint16_t score) {
  VisionPacket p;
  p.kind = 'P';
  p.seq = seq;
  p.receivedMs = now;
  p.found = true;
  p.score = score;
  p.x0 = x;
  p.x1 = x + width;
  p.y0 = 60;
  p.y1 = 140;
  return p;
}
}  // namespace

int main() {
  ImuFilter imu;
  for (int t = 10; t <= 5100; t += 10) imu.update(1, 0, 0, 0, 0, 0, t);
  imu.update(0.819f, 0.574f, 0, 0, 0, 0, 5110);  // one 35-degree acceleration spike
  expect(!imu.state().tiltFault, "brief acceleration tilt must not latch a fault");
  for (int t = 5120; t <= 5330; t += 10) imu.update(0.707f, 0.707f, 0, 0, 0, 0, t);
  expect(!imu.state().tiltFault, "45-degree chassis motion must not stop demo motion");
  for (int t = 5340; t <= 5900; t += 10) imu.update(0.5f, 0.866f, 0, 0, 0, 0, t);
  expect(imu.state().tiltFault, "sustained 60-degree tilt must still stop motion");

  SafetyController manual;
  manual.configureHardware(true, true);
  manual.network(true, 0);
  manual.cameraPacket(0);
  manual.imu(true, true, false, 0);
  expect(!manual.setMode(7, Mode::Manual, 0), "manual accepts mode with healthy IMU");
  expect(!manual.velocity(7, .4, 0, 0, 0), "manual accepts first velocity");
  // CAM alternates gesture/face frames. A single >490 ms gap used to latch
  // IDLE, making the next joystick packet report NOT_IN_MANUAL.
  for (uint64_t t = 80; t <= 640; t += 80) {
    manual.imu(true, true, false, t);
    manual.velocity(7, .4, 0, 0, t);
  }
  expect(manual.snapshot(640).mode == Mode::Manual,
         "manual remains active across a transient camera scheduling gap");

  BoxTrack displayTrack;
  displayTrack.update(face(1, 100, 120, 80, 500));
  displayTrack.update(face(2, 200, 123, 80, 500));
  VisionPacket missed;
  missed.kind = 'P'; missed.seq = 3; missed.receivedMs = 300; missed.found = false;
  displayTrack.update(missed);
  expect(displayTrack.view(650).found,
         "display track holds a recent box through one invalid detector frame");

  PersonFollowController follow;
  for (int i = 1; i <= 3; ++i) follow.update(face(i, i * 100, 120, 80, 500));
  expect(follow.ready(), "stable 0.50-confidence person must be trackable");

  follow.reset();
  for (int i = 1; i <= 3; ++i) follow.update(face(i, i * 100, 120, 80, 900));
  follow.update(face(4, 400, 195, 55, 900));
  follow.update(face(5, 500, 195, 55, 900));
  const auto command = follow.output();
  expect(command.vx > 0.0, "follow must correct distance while centering");
  expect(command.wz > 0.0, "follow must correct horizontal center");
  expect(std::fabs(command.vy) < 0.0001, "follow should not mix lateral drift into camera centering");

  GestureHysteresis gestures(450, 250, 2, 2, 7, 3);
  gestures.update(true, "like", 480);
  gestures.update(true, "like", 470);
  expect(gestures.accepted(), "two stable low-confidence gesture frames must be accepted");

  expect(!gestureActionBoxValid("like", 166, 104, 206, 175),
         "small high-confidence background boxes must not arm motion");
  expect(gestureActionBoxValid("like", 100, 70, 201, 235),
         "observed close-hand boxes must remain eligible for motion");

  // Replay the first four fresh LIKE boxes from the 2026-09-14 field trace.
  // The face target and network were both healthy, but the old 80x120 gate
  // interrupted the four-frame action confirmation on 114-119 px heights.
  const int fieldLikeBoxes[][2] = {{83, 114}, {90, 120}, {83, 114}, {81, 118}};
  GestureActionLatch fieldLikeActions;
  GestureAction fieldLikeResult = GestureAction::None;
  for (const auto& box : fieldLikeBoxes) {
    fieldLikeResult = fieldLikeActions.update(
        gestureActionBoxValid("like", 0, 0, box[0], box[1]), "like");
  }
  expect(fieldLikeResult == GestureAction::StartFollow,
         "four observed real LIKE boxes must start follow");
  expect(!gestureActionBoxValid("two", 0, 0, 83, 114),
         "turn gestures must retain the stricter action box gate");

  GestureActionLatch actions;
  expect(actions.update(true, "like") == GestureAction::None,
         "one filtered frame must not start motion");
  expect(actions.update(false, "like") == GestureAction::None,
         "an intermittent false positive must reset confirmation");
  expect(actions.update(true, "like") == GestureAction::None,
         "like needs a fresh consecutive confirmation");
  expect(actions.update(true, "like") == GestureAction::None,
         "two filtered like frames must not start motion");
  expect(actions.update(true, "like") == GestureAction::None,
         "three filtered like frames must not start motion");
  expect(actions.update(true, "like") == GestureAction::StartFollow,
         "four consecutive filtered like frames start follow");
  expect(actions.update(true, "like") == GestureAction::None,
         "held like fires only once");
  expect(actions.update(false, "like") == GestureAction::None,
         "one dropout does not re-arm the same gesture");
  expect(actions.update(true, "like") == GestureAction::None,
         "same gesture cannot retrigger after one dropout");
  actions.update(false, "no_gesture");
  actions.update(false, "no_gesture");
  actions.update(false, "no_gesture");
  expect(actions.update(true, "dislike") == GestureAction::None,
         "dislike also needs confirmation");
  expect(actions.update(true, "dislike") == GestureAction::Stop,
         "confirmed dislike stops follow");
  expect(actions.update(true, "two") == GestureAction::None,
         "two also needs confirmation");
  expect(actions.update(true, "two") == GestureAction::None,
         "two motion does not arm on two frames");
  expect(actions.update(true, "two") == GestureAction::None,
         "two motion does not arm on three frames");
  expect(actions.update(true, "two") == GestureAction::TurnClockwise,
         "confirmed two turns clockwise");
  expect(actions.update(true, "ok") == GestureAction::None,
         "ok also needs confirmation");
  expect(actions.update(true, "ok") == GestureAction::None,
         "ok motion does not arm on two frames");
  expect(actions.update(true, "ok") == GestureAction::None,
         "ok motion does not arm on three frames");
  expect(actions.update(true, "ok") == GestureAction::TurnCounterClockwise,
         "confirmed ok turns counterclockwise");

  GestureActionLatch threeAction;
  expect(threeAction.update(true, "three") == GestureAction::None,
         "three also needs confirmation");
  expect(threeAction.update(true, "three") == GestureAction::None,
         "three motion does not arm on two frames");
  expect(threeAction.update(true, "three") == GestureAction::None,
         "three motion does not arm on three frames");
  expect(threeAction.update(true, "three") == GestureAction::TurnCounterClockwise,
         "confirmed three turns counterclockwise");

  const PpgWindowQuality observed{24.93f, 0.00004f, 0.395f, 6.44f, 1.469f, 83, 73};
  expect(heartRateCandidateValid(observed), "stable field HR candidate must pass relaxed quality gate");
  expect(spo2CandidateValid(observed), "stable field SpO2 candidate must pass relaxed quality gate");

  float pulse[200];
  float drift[200];
  for (size_t i = 0; i < 200; ++i) {
    const float seconds = static_cast<float>(i) / 25.0f;
    pulse[i] = std::sin(2.0f * 3.14159265358979323846f * 1.35f * seconds) +
               0.25f * std::sin(2.0f * 3.14159265358979323846f * 2.70f * seconds);
    drift[i] = std::sin(2.0f * 3.14159265358979323846f * 0.67f * seconds);
  }
  const auto pulseEstimate = estimatePulsePeriod(pulse, 200, 25.0f);
  expect(pulseEstimate.valid && pulseEstimate.bpm >= 79 && pulseEstimate.bpm <= 83,
         "time-domain pulse estimate must recover an 81 BPM waveform");
  expect(!estimatePulsePeriod(drift, 200, 25.0f).valid,
         "low-frequency pressure drift must not become a boundary heart rate");

  Median5Filter median;
  expect(median.update(96) == 96, "median starts with the first measured value");
  expect(median.update(84) == 90, "median averages the middle pair while filling");
  median.update(95);
  median.update(94);
  expect(median.update(100) == 95, "five-window median suppresses an SpO2 outlier");

  StableMetric persistentHeartRate(2, 30, 12, 3);
  persistentHeartRate.update(true, 78);
  persistentHeartRate.update(true, 78);
  for (int i = 0; i < 21; ++i) persistentHeartRate.update(false, 0);
  expect(persistentHeartRate.valid() && persistentHeartRate.held() &&
             persistentHeartRate.output() == 78,
         "trusted HR remains visible through the observed 21-second pressure drift");
  persistentHeartRate.update(true, 72);
  persistentHeartRate.update(true, 72);
  expect(persistentHeartRate.output() == 75,
         "HR display converges by at most three BPM per valid window");

  StableMetric smoothSpo2(2, 8, 6, 1);
  smoothSpo2.update(true, 90);
  smoothSpo2.update(true, 90);
  smoothSpo2.update(true, 94);
  expect(smoothSpo2.output() == 91,
         "SpO2 display changes by at most one percent per valid window");

  SafetyController autonomous;
  autonomous.network(true, 10);
  autonomous.cameraPacket(10);
  autonomous.person(face(1, 10, 120, 80, 500));
  expect(autonomous.autonomousFollow(10) == nullptr, "like may start follow at 0.50 person confidence");
  expect(autonomous.snapshot(10).mode == Mode::Follow, "autonomous follow enters follow mode");
  autonomous.autonomousStop(20);
  expect(autonomous.snapshot(20).mode == Mode::Idle, "dislike returns to idle");

  autonomous.cameraPacket(30);
  expect(autonomous.autonomousTurn(true, 170.0f, 30) == nullptr, "two may start an IMU turn");
  float yaw = 170.0f;
  for (int i = 1; i <= 18; ++i) {
    yaw += 20.0f;
    if (yaw > 180.0f) yaw -= 360.0f;
    autonomous.updateGestureTurn(yaw, 30 + i * 10);
  }
  expect(autonomous.snapshot(210).mode == Mode::Idle, "IMU-integrated turn stops after 360 degrees");
  expect(!std::strcmp(autonomous.snapshot(210).stopReason, "gesture_turn_complete"), "turn completion is observable");

  if (failures) return 1;
  std::cout << "Field tuning regressions passed\n";
  return 0;
}
