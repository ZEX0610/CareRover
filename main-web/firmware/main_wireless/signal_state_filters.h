#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

class GestureHysteresis {
 public:
  GestureHysteresis(uint16_t enterScoreMilli, uint16_t holdScoreMilli,
                    uint8_t enterFrames, uint8_t switchFrames,
                    uint8_t gestureGraceFrames, uint8_t noHandGraceFrames)
      : enterScoreMilli_(enterScoreMilli),
        holdScoreMilli_(holdScoreMilli),
        enterFrames_(enterFrames),
        switchFrames_(switchFrames),
        gestureGraceFrames_(gestureGraceFrames),
        noHandGraceFrames_(noHandGraceFrames) {
    reset();
  }

  void reset() {
    activeLabel_[0] = '\0';
    candidateLabel_[0] = '\0';
    candidateFrames_ = 0;
    gestureMissFrames_ = 0;
    noHandFrames_ = 0;
    stableScoreMilli_ = 0;
    holding_ = false;
  }

  void update(bool handDetected, const char *label, uint16_t scoreMilli) {
    const bool isGesture = handDetected && label != nullptr &&
                           strcmp(label, "no_gesture") != 0 &&
                           strcmp(label, "no_hand") != 0;
    const bool enterEligible = isGesture && scoreMilli >= enterScoreMilli_;

    if (!accepted()) {
      holding_ = false;
      if (!enterEligible) {
        resetCandidate();
        return;
      }
      updateCandidate(label);
      if (candidateFrames_ >= enterFrames_) activate(label, scoreMilli);
      return;
    }

    if (!handDetected) {
      gestureMissFrames_ = 0;
      resetCandidate();
      if (noHandFrames_ < UINT8_MAX) ++noHandFrames_;
      holding_ = true;
      if (noHandFrames_ > noHandGraceFrames_) clearActive();
      return;
    }

    noHandFrames_ = 0;
    if (isGesture && strcmp(label, activeLabel_) == 0 && scoreMilli >= holdScoreMilli_) {
      gestureMissFrames_ = 0;
      holding_ = false;
      stableScoreMilli_ = scoreMilli;
      resetCandidate();
      return;
    }

    ++gestureMissFrames_;
    holding_ = true;
    if (enterEligible && strcmp(label, activeLabel_) != 0) {
      updateCandidate(label);
    } else {
      resetCandidate();
    }

    if (candidateFrames_ >= switchFrames_) {
      activate(label, scoreMilli);
    } else if (gestureMissFrames_ > gestureGraceFrames_) {
      clearActive();
    }
  }

  bool accepted() const { return activeLabel_[0] != '\0'; }
  bool holding() const { return holding_; }
  const char *label() const { return accepted() ? activeLabel_ : "no_gesture"; }
  uint16_t scoreMilli() const { return stableScoreMilli_; }

 private:
  static constexpr size_t LABEL_CAPACITY = 20;

  void copyLabel(char *destination, const char *source) {
    strncpy(destination, source, LABEL_CAPACITY - 1);
    destination[LABEL_CAPACITY - 1] = '\0';
  }

  void resetCandidate() {
    candidateLabel_[0] = '\0';
    candidateFrames_ = 0;
  }

  void updateCandidate(const char *label) {
    if (strcmp(candidateLabel_, label) == 0) {
      if (candidateFrames_ < UINT8_MAX) ++candidateFrames_;
    } else {
      copyLabel(candidateLabel_, label);
      candidateFrames_ = 1;
    }
  }

  void activate(const char *label, uint16_t scoreMilli) {
    copyLabel(activeLabel_, label);
    stableScoreMilli_ = scoreMilli;
    gestureMissFrames_ = 0;
    noHandFrames_ = 0;
    holding_ = false;
    resetCandidate();
  }

  void clearActive() {
    activeLabel_[0] = '\0';
    stableScoreMilli_ = 0;
    holding_ = false;
    gestureMissFrames_ = 0;
    noHandFrames_ = 0;
    resetCandidate();
  }

  uint16_t enterScoreMilli_;
  uint16_t holdScoreMilli_;
  uint8_t enterFrames_;
  uint8_t switchFrames_;
  uint8_t gestureGraceFrames_;
  uint8_t noHandGraceFrames_;
  char activeLabel_[LABEL_CAPACITY];
  char candidateLabel_[LABEL_CAPACITY];
  uint8_t candidateFrames_;
  uint8_t gestureMissFrames_;
  uint8_t noHandFrames_;
  uint16_t stableScoreMilli_;
  bool holding_;
};

class StableMetric {
 public:
  StableMetric(uint8_t requiredGoodWindows, uint8_t graceBadWindows, int32_t maxDelta,
               int32_t maxOutputStep = 0)
      : requiredGoodWindows_(requiredGoodWindows),
        graceBadWindows_(graceBadWindows),
        maxDelta_(maxDelta),
        maxOutputStep_(maxOutputStep) {
    reset();
  }

  void reset() {
    valid_ = false;
    held_ = false;
    hasPrevious_ = false;
    goodWindows_ = 0;
    badWindows_ = 0;
    previousCandidate_ = 0;
    output_ = 0;
  }

  void update(bool plausible, int32_t candidate) {
    if (!plausible) {
      hasPrevious_ = false;
      goodWindows_ = 0;
      holdOrInvalidate();
      return;
    }

    const bool consistent = hasPrevious_ && labs(candidate - previousCandidate_) <= maxDelta_;
    goodWindows_ = consistent && goodWindows_ < UINT8_MAX
                       ? static_cast<uint8_t>(goodWindows_ + 1)
                       : 1;
    previousCandidate_ = candidate;
    hasPrevious_ = true;

    if (goodWindows_ >= requiredGoodWindows_) {
      int32_t nextOutput = candidate;
      if (valid_ && maxOutputStep_ > 0) {
        const int32_t delta = candidate - output_;
        if (delta > maxOutputStep_) nextOutput = output_ + maxOutputStep_;
        if (delta < -maxOutputStep_) nextOutput = output_ - maxOutputStep_;
      }
      valid_ = true;
      held_ = false;
      badWindows_ = 0;
      output_ = nextOutput;
    } else {
      holdOrInvalidate();
    }
  }

  bool valid() const { return valid_; }
  bool held() const { return held_; }
  int32_t output() const { return output_; }
  uint8_t goodWindows() const { return goodWindows_; }
  uint8_t badWindows() const { return badWindows_; }

 private:
  void holdOrInvalidate() {
    if (!valid_) {
      held_ = false;
      badWindows_ = 0;
      return;
    }
    if (badWindows_ < UINT8_MAX) ++badWindows_;
    if (badWindows_ <= graceBadWindows_) {
      held_ = true;
      return;
    }
    valid_ = false;
    held_ = false;
    output_ = 0;
  }

  uint8_t requiredGoodWindows_;
  uint8_t graceBadWindows_;
  int32_t maxDelta_;
  int32_t maxOutputStep_;
  bool valid_;
  bool held_;
  bool hasPrevious_;
  uint8_t goodWindows_;
  uint8_t badWindows_;
  int32_t previousCandidate_;
  int32_t output_;
};

class Median5Filter {
 public:
  void reset() {
    count_ = 0;
    next_ = 0;
  }

  int32_t update(int32_t value) {
    values_[next_] = value;
    next_ = static_cast<uint8_t>((next_ + 1) % 5);
    if (count_ < 5) ++count_;

    int32_t sorted[5] = {};
    for (uint8_t i = 0; i < count_; ++i) sorted[i] = values_[i];
    for (uint8_t i = 1; i < count_; ++i) {
      const int32_t current = sorted[i];
      uint8_t j = i;
      while (j > 0 && sorted[j - 1] > current) {
        sorted[j] = sorted[j - 1];
        --j;
      }
      sorted[j] = current;
    }
    if ((count_ & 1U) != 0U) return sorted[count_ / 2];
    return (sorted[count_ / 2 - 1] + sorted[count_ / 2]) / 2;
  }

 private:
  int32_t values_[5] = {};
  uint8_t count_ = 0;
  uint8_t next_ = 0;
};
