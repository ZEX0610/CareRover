#include "demo_ppg.h"
#include "gesture_actions.h"
#include <iostream>
#include <string>
using namespace carerover;
int main(){
  GestureDisplay display(tuning::GestureDisplayEnter,tuning::GestureDisplayHold,2,2,7,3);
  GestureActionLatch actions(4,3,tuning::balanced);DemoPpg ppg;
  TimedMetric hr(30000,12,4),spo2(12000,6,1);
  char type;uint64_t ms;
  while(std::cin>>type>>ms){
    if(type=='G'){bool hand;std::string label;int score,x0,y0,x1,y1;std::cin>>hand>>label>>score>>x0>>y0>>x1>>y1;
      display.update(hand,label.c_str(),score,ms);
      const bool eligible=score>=450&&display.accepted()&&!display.holding()&&gestureActionBoxValid(label.c_str(),x0,y0,x1,y1);
      const auto action=actions.update(eligible,display.label(),ms);
      std::cout<<"G "<<ms<<" "<<int(action)<<" "<<display.accepted()<<" "<<display.holding()<<"\n";
    }else if(type=='R'){uint32_t red,ir;std::cin>>red>>ir;if(ppg.sample(red,ir,ms))std::cout<<"R "<<ms<<" "<<ppg.hr.valid()<<" "<<ppg.hr.output()<<" "<<ppg.spo2.valid()<<" "<<ppg.spo2.output()<<" "<<ppg.result.quality<<"\n";
    }else if(type=='H'){bool finger,hv,sv;int h,s;std::cin>>finger>>hv>>h>>sv>>s;if(!finger){hr.reset();spo2.reset();}else{hr.update(hv,h,.5,ms);spo2.update(sv,s,.5,ms);}
      std::cout<<"H "<<ms<<" "<<hr.valid()<<" "<<hr.output()<<" "<<spo2.valid()<<" "<<spo2.output()<<"\n";
    }else return 2;
  }
}
