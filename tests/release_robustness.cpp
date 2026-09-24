#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
constexpr int N=512; constexpr double sr=48000.0,pi=3.14159265358979323846;
void setP(ParameterChanges& c,ParamID id,double v){int32 qi=0;auto*q=c.addParameterData(id,qi);if(!q)throw 10;int32 pi=0;if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;}
std::vector<double> render(ProcessModes mode,bool restart,bool injectBad,bool extreme){
 auto p=std::make_unique<MixEngine::Processor>(); ProcessSetup s{};s.processMode=mode;s.symbolicSampleSize=kSample64;s.maxSamplesPerBlock=N;s.sampleRate=sr;
 if(p->setupProcessing(s)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 20;
 if(restart){if(p->setProcessing(false)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 21;}
 ParameterChanges c{64},o{8};
 const std::pair<ParamID,double> cfg[]={
  {MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},{MixEngine::kParamCalibration,0.5},{MixEngine::kParamAutoGain,1.0},
  {MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleMode,2.0/3.0},{MixEngine::kParamConsoleDrive,0.7},{MixEngine::kParamConsoleNoise,0.0},
  {MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeAmount,0.65},{MixEngine::kParamTubeType,0.5},
  {MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.6},{MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,0.9},{MixEngine::kParamTapeHiss,0.0},
  {MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,0.55},{MixEngine::kParamGlueCharacter,0.5},
  {MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.55},{MixEngine::kParamVinylWear,0.3},{MixEngine::kParamVinylNoise,0.0},
  {MixEngine::kParamDepth,0.5},{MixEngine::kParamWidth,0.5},{MixEngine::kParamLowMono,0.0},{MixEngine::kParamQuality,0.5}
 };
 for(auto [id,v]:cfg)setP(c,id,v);
 std::array<double,N> in{},out{}; for(int n=0;n<N;++n){double t=double(n)/sr;in[n]=(extreme?24.0:0.2)*(std::sin(2*pi*997*t)+0.35*std::sin(2*pi*5111*t));}
 if(injectBad){in[64]=std::numeric_limits<double>::quiet_NaN();in[65]=std::numeric_limits<double>::infinity();in[66]=-std::numeric_limits<double>::infinity();}
 double*ip[1]{in.data()};double*op[1]{out.data()};AudioBusBuffers ib{},ob{};ib.numChannels=1;ib.channelBuffers64=ip;ob.numChannels=1;ob.channelBuffers64=op;
 ProcessData d{};d.processMode=mode;d.symbolicSampleSize=kSample64;d.numSamples=N;d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;d.inputParameterChanges=&c;d.outputParameterChanges=&o;
 if(p->process(d)!=kResultOk)throw 22; return {out.begin(),out.end()};
}
double maxDiff(const std::vector<double>&a,const std::vector<double>&b){double m=0;for(size_t i=0;i<a.size();++i)m=std::max(m,std::abs(a[i]-b[i]));return m;}
bool finiteTail(const std::vector<double>&x,int start){for(int i=start;i<(int)x.size();++i)if(!std::isfinite(x[(size_t)i]))return false;return true;}
}
int main(){try{
 const auto rt=render(kRealtime,false,false,false),off=render(kOffline,false,false,false),restart=render(kRealtime,true,false,false),extreme=render(kRealtime,false,false,true),bad=render(kRealtime,false,true,false);
 const double offlineDiff=maxDiff(rt,off),restartDiff=maxDiff(rt,restart);
 double peak=0;for(double v:extreme){if(!std::isfinite(v)){std::cerr<<"Extreme finite-input produced non-finite output\n";return 1;}peak=std::max(peak,std::abs(v));}
 const bool recovered=finiteTail(bad,160);
 std::cout<<"Offline/realtime maxDiff="<<offlineDiff<<" restart maxDiff="<<restartDiff<<" extremePeak="<<peak<<" badInputRecovered="<<(recovered?"YES":"NO")<<"\n";
 bool ok=offlineDiff<=1e-12 && restartDiff<=1e-12 && peak<100.0 && recovered;
 std::cout<<(ok?"PASS":"FAIL")<<": release robustness (offline/lifecycle/extreme/non-finite recovery)\n";
 return ok?0:1;
}catch(int c){std::cerr<<"Release robustness setup FAIL "<<c<<"\n";return c;}catch(...){return 90;}}
