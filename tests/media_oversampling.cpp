#include "oversampling.h"
#include "media_oversampling_live.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double kPi=3.14159265358979323846;
constexpr double kSampleRate=48000.0;
constexpr int kCount=65536;
constexpr int kWarmup=4096;
using MixEngine::OversamplingEngine;

double tapeCore(double x,double shape){
 const double k=std::max(0.0,shape-1.0),vv=x*x;
 return x/std::sqrt(1.0+(1.35*k)*vv);
}
double vinylCore(double x,double drive,double material){
 const double m=std::clamp(material,0.0,1.0),bias=0.16*m;
 const double center=std::tanh(bias*drive);
 const double slope=drive*(1.0-center*center);
 const double y=std::tanh((x+bias)*drive)-center;
 return slope>1.0e-12?y/slope:x;
}

template<class Fn>
double residualRms(Fn&& fn,int factor,double frequency){
 OversamplingEngine e;
 std::vector<double> y(kCount);
 for(int n=0;n<kCount;++n){
  const double x=0.95*std::sin(2.0*kPi*frequency*n/kSampleRate);
  y[n]=e.process(x,factor,fn);
  if(!std::isfinite(y[n]))return 1e9;
 }
 double ss=0,cc=0,sc=0,sy=0,cy=0;
 for(int n=kWarmup;n<kCount;++n){
  const double s=std::sin(2*kPi*frequency*n/kSampleRate),c=std::cos(2*kPi*frequency*n/kSampleRate);
  ss+=s*s;cc+=c*c;sc+=s*c;sy+=s*y[n];cy+=c*y[n];
 }
 const double det=ss*cc-sc*sc;
 const double a=(sy*cc-cy*sc)/det,b=(cy*ss-sy*sc)/det;
 double err=0;int count=0;
 for(int n=kWarmup;n<kCount;++n){
  const double fit=a*std::sin(2*kPi*frequency*n/kSampleRate)+b*std::cos(2*kPi*frequency*n/kSampleRate);
  const double d=y[n]-fit;err+=d*d;++count;
 }
 return std::sqrt(err/count);
}

bool verifyTape(){
 bool ok=true;
 for(double shape:{1.0,1.409375,2.30}){
  OversamplingEngine eco;int cur=1;
  for(int i=0;i<20000;++i){
   const double x=1.7*std::sin(0.013*i)+0.31*std::sin(0.071*i);
   const double direct=tapeCore(x,shape);
   const double live=MixEngine::processTapeOversampledCore(eco,cur,1,x,shape);
   if(direct!=live){ok=false;break;}
  }
  for(double f:{9000.0,15000.0}){
   const auto fn=[=](double v){return tapeCore(v,shape);};
   const double r1=residualRms(fn,1,f),r2=residualRms(fn,2,f),r4=residualRms(fn,4,f);
   const bool identity=std::abs(shape-1.0)<1e-12;
   const bool pass=identity?(r1<1e-8&&r2<1e-8&&r4<1e-8):(r2<r1&&r4<=r2*1.01);
   std::cout<<"Tape shape="<<shape<<" f="<<f<<" residual "<<r1<<" "<<r2<<" "<<r4<<" "<<(pass?"PASS":"FAIL")<<"\n";
   ok&=pass;
  }
 }
 return ok;
}

bool verifyVinyl(){
 bool ok=true;
 for(const auto p: {std::pair<double,double>{1.0,0.0},{1.45,0.5},{2.25,1.0}}){
  const double drive=p.first,material=p.second;
  OversamplingEngine eco;int cur=1;
  for(int i=0;i<20000;++i){
   const double x=1.7*std::sin(0.013*i)+0.31*std::sin(0.071*i);
   const double direct=vinylCore(x,drive,material);
   const double live=MixEngine::processVinylOversampledCore(eco,cur,1,x,drive,material);
   if(std::abs(direct-live)>1e-15){ok=false;break;}
  }
  for(double f:{9000.0,15000.0}){
   const auto fn=[=](double v){return vinylCore(v,drive,material);};
   const double r1=residualRms(fn,1,f),r2=residualRms(fn,2,f),r4=residualRms(fn,4,f);
   const bool pass=(drive==1.0&&material==0.0)?(r1<1e-8&&r2<1e-8&&r4<1e-8):(r2<r1&&r4<=r2*1.01);
   std::cout<<"Vinyl drive="<<drive<<" material="<<material<<" f="<<f<<" residual "<<r1<<" "<<r2<<" "<<r4<<" "<<(pass?"PASS":"FAIL")<<"\n";
   ok&=pass;
  }
 }
 return ok;
}
}

int main(){
 const bool tape=verifyTape(),vinyl=verifyVinyl();
 if(!(tape&&vinyl)){
  std::cerr<<"FAILED: merged FINAL/V2 Tape/Vinyl oversampling verification\n";
  return 1;
 }
 std::cout<<"PASSED: merged FINAL/V2 Tape/Vinyl Eco equivalence and alias reduction\n";
 return 0;
}
