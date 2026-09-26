#pragma once

#include <cctype>
#include <cstdint>
#include <cstring>
#include "demo_tuning.h"

namespace carerover {

enum class GestureAction { None, StartFollow, Stop, TurnClockwise, TurnCounterClockwise };

inline bool gestureActionBoxValid(const char* label, int x0, int y0, int x1, int y1) {
  constexpr int kMinActionBoxWidth = 80;
  constexpr int kMinActionBoxHeight = 120;
  // Starting follow is additionally guarded by a fresh person target, an
  // online supervising station, a healthy IMU and the four-frame latch.  The
  // field trace shows a real shoulder-level LIKE at 114-119 px high, so only
  // LIKE gets a slightly lower height gate.  Unattended turn actions retain
  // the stricter dimensions established by the static-background evidence.
  const bool isLike = label &&
                      (!std::strcmp(label, "like") || !std::strcmp(label, "LIKE"));
  const int minHeight = isLike ? 110 : kMinActionBoxHeight;
  return x1 > x0 && y1 > y0 && x1 - x0 >= kMinActionBoxWidth &&
         y1 - y0 >= minHeight;
}

class GestureActionLatch {
 public:
  GestureActionLatch(uint8_t confirmationFrames = 4, uint8_t releaseFrames = 3, bool windowed = false)
      : windowed_(windowed), confirmationFrames_(confirmationFrames), releaseFrames_(releaseFrames) {}

  GestureAction update(bool accepted, const char* label, uint64_t now=0) {
    if(windowed_) return updateWindow(accepted,label,now);
    if (!accepted || !label || !label[0]) {
      candidate_[0] = '\0';
      candidateFrames_ = 0;
      if (active_[0] && releasedFrames_ < UINT8_MAX) ++releasedFrames_;
      if (releasedFrames_ >= releaseFrames_) active_[0] = '\0';
      return GestureAction::None;
    }
    char normalized[16] = {};
    size_t i = 0;
    for (; label[i] && i + 1 < sizeof(normalized); ++i) {
      normalized[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[i])));
    }
    releasedFrames_ = 0;
    if (!std::strcmp(active_, normalized)) {
      candidate_[0] = '\0';
      candidateFrames_ = 0;
      return GestureAction::None;
    }
    if (!std::strcmp(candidate_, normalized)) {
      if (candidateFrames_ < UINT8_MAX) ++candidateFrames_;
    } else {
      std::strcpy(candidate_, normalized);
      candidateFrames_ = 1;
    }
    // Stopping should remain responsive.  Any gesture that can start motion
    // deliberately needs the longer confirmation window.
    const uint8_t requiredFrames = !std::strcmp(normalized, "DISLIKE")
                                       ? static_cast<uint8_t>(2)
                                       : confirmationFrames_;
    if (candidateFrames_ < requiredFrames) return GestureAction::None;
    std::strcpy(active_, normalized);
    candidate_[0] = '\0';
    candidateFrames_ = 0;
    if (!std::strcmp(normalized, "LIKE")) return GestureAction::StartFollow;
    if (!std::strcmp(normalized, "DISLIKE")) return GestureAction::Stop;
    if (!std::strcmp(normalized, "TWO")) return GestureAction::TurnClockwise;
    if (!std::strcmp(normalized, "THREE")) return GestureAction::TurnCounterClockwise;
    if (!std::strcmp(normalized, "OK")) return GestureAction::TurnCounterClockwise;
    return GestureAction::None;
  }

 private:
  GestureAction updateWindow(bool accepted,const char* label,uint64_t now) {
    char normalized[16]={};
    if(label)for(size_t i=0;label[i]&&i<15;++i)normalized[i]=static_cast<char>(std::toupper(static_cast<unsigned char>(label[i])));
    for(int i=4;i>0;--i)votes_[i]=votes_[i-1];
    votes_[0]={};if(accepted){std::strcpy(votes_[0].label,normalized);votes_[0].ms=now;}
    if(!accepted || std::strcmp(active_,normalized)) {if(releasedFrames_<255)++releasedFrames_;if(releasedFrames_>=releaseFrames_)active_[0]=0;}
    else releasedFrames_=0;
    if(!accepted||!normalized[0]||!std::strcmp(active_,normalized))return GestureAction::None;
    const bool stop=!std::strcmp(normalized,"DISLIKE"),like=!std::strcmp(normalized,"LIKE");
    const unsigned window=stop?3:like?4:5,required=stop?2:like?3:4;
    unsigned votes=0;for(unsigned i=0;i<window;++i)if(!std::strcmp(votes_[i].label,normalized)&&now>=votes_[i].ms&&now-votes_[i].ms<=1500)++votes;
    if(votes<required)return GestureAction::None;
    std::strcpy(active_,normalized);releasedFrames_=0;
    if(like)return GestureAction::StartFollow;
    if(stop)return GestureAction::Stop;
    if(!std::strcmp(normalized,"TWO"))return GestureAction::TurnClockwise;
    if(!std::strcmp(normalized,"THREE"))return GestureAction::TurnCounterClockwise;
    if(!std::strcmp(normalized,"OK"))return GestureAction::TurnCounterClockwise;
    return GestureAction::None;
  }
  struct Vote {char label[16]={};uint64_t ms=0;}votes_[5];
  bool windowed_=false;
  char active_[16] = {};
  char candidate_[16] = {};
  uint8_t candidateFrames_ = 0;
  uint8_t releasedFrames_ = 0;
  uint8_t confirmationFrames_;
  uint8_t releaseFrames_;
};

}  // namespace carerover
