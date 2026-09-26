#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace carerover {
struct VisionPacket {
  char kind = 'P', label[24] = "no_hand";
  uint32_t seq = 0, camMs = 0, inferMs = 0;
  bool found = false;
  uint16_t score = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  uint64_t receivedMs = 0;
};
inline uint8_t visionCrc(const char* p, size_t n) {
  uint8_t crc = 0;
  while (n--) { crc ^= uint8_t(*p++); for (int b=0;b<8;++b) crc = crc&128 ? uint8_t((crc<<1)^7) : uint8_t(crc<<1); }
  return crc;
}
inline bool decimal(const char* s, uint32_t& n) {
  if (!s || !*s) return false;
  uint64_t v=0;
  for (;*s;++s) { if (*s<'0'||*s>'9') return false; v=v*10+unsigned(*s-'0'); if(v>UINT32_MAX) return false; }
  n=uint32_t(v); return true;
}
inline int hexDigit(char c) { return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1; }
inline bool parseVision(const char* input, VisionPacket& out) {
  const size_t len=strlen(input);
  if(len<8||len>=256||input[0]!='@') return false;
  const char* star=strchr(input,'*');
  if(!star || star-input!=int(len)-3 || hexDigit(star[1])<0 || hexDigit(star[2])<0) return false;
  if(visionCrc(input+1,size_t(star-input-1))!=uint8_t(hexDigit(star[1])*16+hexDigit(star[2]))) return false;
  char body[256]; const size_t n=size_t(star-input-1); memcpy(body,input+1,n); body[n]=0;
  char* fields[12]={body}; size_t count=1;
  for(size_t i=0;i<n;++i) if(body[i]==',') { body[i]=0; if(count==12) return false; fields[count++]=body+i+1; }
  const bool gesture=!strcmp(fields[0],"G");
  if((!gesture&&strcmp(fields[0],"P")) || count!=(gesture?11U:10U)) return false;
  for(size_t i=0;i<count;++i) if(!*fields[i]) return false;
  VisionPacket p; p.kind=gesture?'G':'P';
  if(gesture) {
    const size_t l=strlen(fields[4]); if(l>=sizeof(p.label)) return false;
    for(size_t i=0;i<l;++i) if(!((fields[4][i]>='a'&&fields[4][i]<='z')||(fields[4][i]>='A'&&fields[4][i]<='Z')||fields[4][i]=='_')) return false;
    memcpy(p.label,fields[4],l+1);
  }
  uint32_t flag=0, nums[6]={}; const int base=gesture?5:4;
  if(!decimal(fields[1],p.seq)||!decimal(fields[2],p.camMs)||!decimal(fields[3],flag)||flag>1) return false;
  for(int i=0;i<6;++i) if(!decimal(fields[base+i],nums[i])) return false;
  if(nums[0]>1000||nums[1]>320||nums[2]>240||nums[3]>320||nums[4]>240||nums[5]>60000) return false;
  p.found=flag; p.score=nums[0]; p.x0=nums[1]; p.y0=nums[2]; p.x1=nums[3]; p.y1=nums[4]; p.inferMs=nums[5];
  if(p.found) { if(p.x1<=p.x0||p.y1<=p.y0) return false; }
  else if(p.score||p.x0||p.y0||p.x1||p.y1) return false;
  out=p; return true;
}
inline bool encodeVision(const VisionPacket& p, char* out, size_t size) {
  char body[224]; int n;
  if(p.kind=='G') n=snprintf(body,sizeof(body),"G,%lu,%lu,%u,%s,%u,%u,%u,%u,%u,%lu",(unsigned long)p.seq,(unsigned long)p.camMs,p.found,p.label,p.score,p.x0,p.y0,p.x1,p.y1,(unsigned long)p.inferMs);
  else n=snprintf(body,sizeof(body),"P,%lu,%lu,%u,%u,%u,%u,%u,%u,%lu",(unsigned long)p.seq,(unsigned long)p.camMs,p.found,p.score,p.x0,p.y0,p.x1,p.y1,(unsigned long)p.inferMs);
  if(n<0||size_t(n)>=sizeof(body)) return false;
  int written=snprintf(out,size,"@%s*%02X",body,visionCrc(body,size_t(n)));
  VisionPacket checked;
  return written>0&&size_t(written)<size&&parseVision(out,checked);
}
inline float boxIou(const VisionPacket& a, const VisionPacket& b) {
  const int x0=a.x0>b.x0?a.x0:b.x0, y0=a.y0>b.y0?a.y0:b.y0;
  const int x1=a.x1<b.x1?a.x1:b.x1, y1=a.y1<b.y1?a.y1:b.y1;
  const float intersection=float((x1>x0?x1-x0:0)*(y1>y0?y1-y0:0));
  const float area=float((a.x1-a.x0)*(a.y1-a.y0)+(b.x1-b.x0)*(b.y1-b.y0))-intersection;
  return area>0?intersection/area:0;
}
// One bounded line at a time. Overflow discards the entire line, not its suffix.
class CamVisionAdapter {
 public:
  uint32_t valid=0, bad=0, stale=0, resets=0;
  bool resynchronized=false;
  bool feed(char c, uint64_t now, VisionPacket& out) {
    resynchronized=false;
    if(c=='\0') { overflow_=true; return false; }
    if(c=='\r') return false;
    if(c!='\n') { if(length_+1<sizeof(line_)&&!overflow_) line_[length_++]=c; else overflow_=true; return false; }
    if(overflow_) { ++bad; length_=0; overflow_=false; return false; }
    line_[length_]=0; const size_t n=length_; length_=0;
    if(!n) return false;
    VisionPacket p; if(!parseVision(line_,p)) { ++bad; return false; }
    // Both kinds share the CAM's sequence. Re-sync only after a source timeout;
    // cached/replayed packets cannot continually renew an active control lease.
    if(seen_ && (int32_t(p.seq-seq_)<=0 || int32_t(p.camMs-camMs_)<0)) {
      if(now-lastMs_<500) { ++stale; return false; }
      ++resets; resynchronized=true;
    }
    seen_=true; seq_=p.seq; camMs_=p.camMs; lastMs_=now;
    p.receivedMs=now; out=p; ++valid; return true;
  }
 private:
  char line_[256]={}; size_t length_=0; bool overflow_=false, seen_=false;
  uint32_t seq_=0, camMs_=0; uint64_t lastMs_=0;
};
}
