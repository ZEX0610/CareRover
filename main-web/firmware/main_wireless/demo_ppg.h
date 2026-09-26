#pragma once
#include "demo_signal_filters.h"
#include "ppg_rate_estimator.h"
namespace carerover {
struct DemoPpgResult {bool hrCandidate=false,spo2Candidate=false;int hr=0,spo2=0;float quality=0,hz=0,acDc=0,corr=0,snr=0,ratio=0,pulseCorr=0,irMean=0;};
class DemoPpg {
 public:
  TimedMetric hr{30000,12,4},spo2{12000,6,1};
  DemoPpgResult result;
  void reset(){hr.reset();spo2.reset();result={};total_=count_=0;last_=lastSpo2_=0;dcIr_=dcRed_=0;}
  bool sample(uint32_t red,uint32_t ir,uint64_t now){
    if(last_&&(now<last_||now-last_>250)){reset();}
    const float dt=last_&&now>last_?float(now-last_)/1000:.04f;last_=now;
    if(!count_){dcIr_=ir;dcRed_=red;}
    const float alpha=dt/(1.8f+dt);dcIr_+=alpha*(ir-dcIr_);dcRed_+=alpha*(red-dcRed_);
    const auto i=total_%200;rawIr_[i]=ir;rawRed_[i]=red;times_[i]=now;
    acIr_[i]=ir-dcIr_;acRed_[i]=red-dcRed_;++total_;count_=std::min(200u,count_+1);
    if(count_<150 || (total_-150)%13)return false;
    result=analyze(150);hr.update(result.hrCandidate,result.hr,result.quality,now);
    if(count_>=200 && (lastSpo2_==0 || now-lastSpo2_>=950)){
      const auto oxygen=analyze(200);lastSpo2_=now;
      spo2.update(oxygen.spo2Candidate,oxygen.spo2,oxygen.quality,now);
      result.spo2Candidate=oxygen.spo2Candidate;result.spo2=oxygen.spo2;result.ratio=oxygen.ratio;
    }
    return true;
  }
 private:
  static float unit(float x,float low,float high){return std::clamp((x-low)/(high-low),0.f,1.f);}
  DemoPpgResult analyze(unsigned n){
    DemoPpgResult q;float ir[200]={},red[200]={};double irMean=0,redMean=0;
    auto idx=[&](unsigned k){return (total_-n+k)%200;};
    for(unsigned j=0;j<n;++j){irMean+=rawIr_[idx(j)];redMean+=rawRed_[idx(j)];}
    irMean/=n;redMean/=n;q.irMean=irMean;
    // Remove the residual local trend before autocorrelation; median rejects isolated impulses.
    for(unsigned j=0;j<n;++j){float medI[3],medR[3];for(int k=-1;k<=1;++k){const auto pos=idx(unsigned(std::clamp(int(j)+k,0,int(n)-1)));medI[k+1]=acIr_[pos];medR[k+1]=acRed_[pos];}std::sort(medI,medI+3);std::sort(medR,medR+3);ir[j]=medI[1];red[j]=medR[1];}
    float meanI=0,meanR=0;for(unsigned j=0;j<n;++j){meanI+=ir[j]/n;meanR+=red[j]/n;}
    double energyI=0,energyR=0,dot=0;
    for(unsigned j=0;j<n;++j){ir[j]-=meanI;red[j]-=meanR;energyI+=ir[j]*ir[j];energyR+=red[j]*red[j];dot+=ir[j]*red[j];}
    const auto duration=times_[idx(n-1)]-times_[idx(0)];q.hz=duration?float(n-1)*1000/duration:0;
    q.acDc=irMean>0?std::sqrt(energyI/n)/irMean:0;
    q.corr=energyI*energyR>0?dot/std::sqrt(energyI*energyR):0;
    q.ratio=q.acDc>0&&redMean>0?(std::sqrt(energyR/n)/redMean)/q.acDc:0;
    const auto pulse=estimatePulsePeriod(ir,n,q.hz,45,150,.22f);q.hr=pulse.bpm;q.pulseCorr=pulse.correlation;
    double sum=0,best=0;unsigned bins=0;
    if(q.hz>=20&&q.hz<=30)for(float f=.75f;f<=2.5f;f+=.05f){double re=0,im=0;for(unsigned j=0;j<n;++j){const float phase=6.2831853f*f*j/q.hz;re+=ir[j]*std::cos(phase);im+=ir[j]*std::sin(phase);}const double power=re*re+im*im;sum+=power;best=std::max(best,power);++bins;}
    q.snr=sum>0?best*bins/sum:0;
    q.quality=.30f*unit(q.pulseCorr,.22f,.55f)+.25f*unit(q.snr,1.5f,4.f)+.20f*unit(q.acDc,.00001f,.00005f)+.15f*unit(q.corr,.20f,.70f)+.10f*unit(irMean,20000,90000);
    const bool waveform=q.hz>=20&&q.hz<=30&&q.acDc>=.00001f&&q.acDc<=.05f&&q.snr>=1.5f;
    q.hrCandidate=waveform&&pulse.valid&&q.hr>=45&&q.hr<=150&&q.quality>=.42f;
    q.spo2=std::lround(110-25*q.ratio);
    q.spo2Candidate=waveform&&q.corr>=.20f&&q.ratio>=.15f&&q.ratio<=2&&q.spo2>=65&&q.spo2<=100&&q.quality>=.38f;
    return q;
  }
  unsigned total_=0,count_=0;uint64_t last_=0,lastSpo2_=0,times_[200]={};
  float dcIr_=0,dcRed_=0,acIr_[200]={},acRed_[200]={};uint32_t rawIr_[200]={},rawRed_[200]={};
};
}
