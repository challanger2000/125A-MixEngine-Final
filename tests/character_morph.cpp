#include "character_morph.h"
#include "nonlinear_cores.h"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace MixEngine;
namespace {
bool close(double a,double b,double eps=1.0e-12){return std::abs(a-b)<=eps;}
bool finite(double v){return std::isfinite(v);}
}
int main(){
 bool ok=true;
 const auto u=tubeCharacter(0.0),t=tubeCharacter(0.5),x=tubeCharacter(1.0);
 ok&=close(u.gain,1.45)&&close(u.bias,0.020)&&close(u.asym,0.012)&&close(u.secondStage,0.12)&&close(u.autoGainDb,1.38);
 ok&=close(t.gain,1.85)&&close(t.bias,0.038)&&close(t.asym,0.022)&&close(t.secondStage,0.18)&&close(t.autoGainDb,2.39);
 ok&=close(x.gain,2.30)&&close(x.bias,0.060)&&close(x.asym,0.036)&&close(x.secondStage,0.25)&&close(x.autoGainDb,3.31);

 const auto s=tapeCharacter(0.0),m=tapeCharacter(0.5),f=tapeCharacter(1.0);
 ok&=close(s.cutoffHz,11000.0)&&close(s.bumpFreqHz,65.0)&&close(s.bumpAmount,0.045)&&close(s.wowHz,0.42)&&close(s.flutterHz,5.2)&&close(s.hissTone,0.80);
 ok&=close(m.cutoffHz,15000.0)&&close(m.bumpFreqHz,80.0)&&close(m.bumpAmount,0.025)&&close(m.wowHz,0.50)&&close(m.flutterHz,6.0)&&close(m.hissTone,1.00);
 ok&=close(f.cutoffHz,19000.0)&&close(f.bumpFreqHz,100.0)&&close(f.bumpAmount,0.010)&&close(f.wowHz,0.58)&&close(f.flutterHz,6.8)&&close(f.hissTone,1.12);

 double prevTube=processTubeNonlinearCore(0.63,0.0,0.82),maxTubeStep=0.0;
 double prevCut=tapeCharacter(0.0).cutoffHz,maxCutStep=0.0;
 for(int i=1;i<=2000;++i){
  const double p=double(i)/2000.0;
  const double y=processTubeNonlinearCore(0.63,p,0.82);
  const auto tc=tapeCharacter(p);
  if(!finite(y)||!finite(tc.cutoffHz)||!finite(tc.bumpFreqHz)||!finite(tc.wowHz)||!finite(tc.flutterHz))ok=false;
  maxTubeStep=std::max(maxTubeStep,std::abs(y-prevTube));
  maxCutStep=std::max(maxCutStep,std::abs(tc.cutoffHz-prevCut));
  prevTube=y;prevCut=tc.cutoffHz;
 }
 const double eps=1.0e-8;
 const double tubeJump=std::abs(processTubeNonlinearCore(0.63,0.5+eps,0.82)-processTubeNonlinearCore(0.63,0.5-eps,0.82));
 const double tapeJump=std::abs(tapeCharacter(0.5+eps).cutoffHz-tapeCharacter(0.5-eps).cutoffHz);
 ok&=tubeJump<1.0e-6;
 ok&=tapeJump<1.0e-3;
 ok&=maxTubeStep<0.01;
 ok&=maxCutStep<=4.0;
 std::cout<<"Tube max step="<<maxTubeStep<<" boundary jump="<<tubeJump<<"\n";
 std::cout<<"Tape cutoff max step="<<maxCutStep<<" boundary jump="<<tapeJump<<"\n";
 std::cout<<(ok?"PASS":"FAIL")<<": FINAL-anchor continuous Tube/Tape morphs\n";
 return ok?0:1;
}
