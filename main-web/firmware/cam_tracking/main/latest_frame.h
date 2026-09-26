#pragma once
#include <cstddef>
#include <cstdint>
// Call under a mutex. A reader lease pins a slot; producers never overwrite it.
class LatestFrame {
 public:
  int writable() const { for(int i=0;i<3;++i)if(i!=latest_&&i!=reader_)return i;return -1; }
  bool publish(int index,size_t bytes) { if(index<0||index>=3||index==reader_||!bytes||bytes>Capacity)return false;length_[index]=bytes;latest_=index;++sequence_;return true; }
  int acquire(uint32_t last) { if(reader_>=0||latest_<0||sequence_==last)return -1;reader_=latest_;return reader_; }
  void release(int i) {if(i==reader_)reader_=-1;}
  size_t length(int i) const{return i>=0&&i<3?length_[i]:0;}
  uint32_t sequence() const{return sequence_;}
  static constexpr size_t Capacity=96*1024;
 private:
  int latest_=-1,reader_=-1;size_t length_[3]={};uint32_t sequence_=0;
};
