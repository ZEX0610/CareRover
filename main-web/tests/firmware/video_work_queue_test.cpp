#include "video_work_queue.h"
#include <cassert>
#include <iostream>

int main() {
  VideoWorkQueue q;
  assert(q.beginRead() == -1);
  const int first = q.beginWrite();
  assert(first >= 0 && q.beginRead() == -1);
  assert(q.finishWrite(first));
  const int second = q.beginWrite();
  assert(second >= 0 && second != first);
  assert(q.finishWrite(second) && q.replaced() == 1);
  const int reading = q.beginRead();
  assert(reading == second);
  const int writing = q.beginWrite();
  assert(writing == first && q.beginRead() == -1);
  assert(q.finishWrite(writing));
  q.endRead(reading);
  assert(q.beginRead() == writing);
  q.endRead(writing);
  assert(q.beginWrite() >= 0);
  std::cout << "Video queue lease and newest-frame tests passed\n";
}
