#pragma once

// Single producer, single consumer; callers hold the same mutex around each
// transition. Writing and reading are intentionally outside the mutex.
class VideoWorkQueue {
 public:
  int beginWrite() {
    for (int i = 0; i < 2; ++i) {
      if (state_[i] == Free) { state_[i] = Writing; return i; }
    }
    if (pending_ >= 0) {
      const int i = pending_;
      pending_ = -1;
      state_[i] = Writing;
      ++replaced_;
      return i;
    }
    return -1;
  }

  bool finishWrite(int i) {
    if (i < 0 || i >= 2 || state_[i] != Writing) return false;
    if (pending_ >= 0) { state_[pending_] = Free; ++replaced_; }
    state_[i] = Pending;
    pending_ = i;
    return true;
  }

  void cancelWrite(int i) {
    if (i >= 0 && i < 2 && state_[i] == Writing) state_[i] = Free;
  }

  int beginRead() {
    if (pending_ < 0) return -1;
    const int i = pending_;
    pending_ = -1;
    state_[i] = Reading;
    return i;
  }

  void endRead(int i) {
    if (i >= 0 && i < 2 && state_[i] == Reading) state_[i] = Free;
  }

  unsigned replaced() const { return replaced_; }

 private:
  enum State { Free, Writing, Pending, Reading };
  State state_[2] = { Free, Free };
  int pending_ = -1;
  unsigned replaced_ = 0;
};
