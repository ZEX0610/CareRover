#pragma once
#include <Preferences.h>
#include "continuous_servo_drive.h"
#include "motion_layout.h"
namespace carerover {
inline bool loadMotionCalibration(ServoCalibration& c) {
  Preferences nvs; if(!nvs.begin("cr-motion",true)) return false;
  bool verified=nvs.getBool("verified",false)&&nvs.getUInt("layout",0)==MOTION_LAYOUT_ID;
  c.speedSpanUs=nvs.getUShort("span",300);
  for(int i=0;i<4;++i) {
    char key[8];snprintf(key,sizeof(key),"n%d",i);c.neutralUs[i]=nvs.getUShort(key,1500);
    snprintf(key,sizeof(key),"d%d",i);c.directionSign[i]=nvs.getChar(key,1);
    snprintf(key,sizeof(key),"s%d",i);c.wheelSpanUs[i]=nvs.getUShort(key,c.speedSpanUs);
    verified=verified&&c.neutralUs[i]>=1300&&c.neutralUs[i]<=1700&&(c.directionSign[i]==1||c.directionSign[i]==-1)&&c.wheelSpanUs[i]>=100&&c.wheelSpanUs[i]<=500;
  }
  nvs.end();return verified;
}
}
