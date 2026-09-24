#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
std::vector<double> loadState(int count){
 auto p=std::make_unique<MixEngine::Processor>();
 MemoryStream stream;
 IBStreamer w(&stream,kLittleEndian);
 std::array<double,MixEngine::kParamCount> src{};
 for(int i=0;i<MixEngine::kParamCount;++i)src[(size_t)i]=0.05+0.025*i;
 src[MixEngine::kParamConsoleNoise]=0.37;
 src[MixEngine::kParamTubeType]=0.72;
 src[MixEngine::kParamMeterSource]=0.0;
 src[MixEngine::kParamTapeHiss]=0.66;
 src[MixEngine::kParamVinylNoise]=0.44;
 for(int i=0;i<count;++i)if(!w.writeDouble(src[(size_t)i]))throw 10;
 if(stream.seek(0,IBStream::kIBSeekSet,nullptr)!=kResultOk)throw 11;
 if(p->setState(&stream)!=kResultOk)throw 12;
 MemoryStream out;
 if(p->getState(&out)!=kResultOk)throw 13;
 if(out.seek(0,IBStream::kIBSeekSet,nullptr)!=kResultOk)throw 14;
 IBStreamer r(&out,kLittleEndian);
 std::vector<double> v(MixEngine::kParamCount);
 for(int i=0;i<MixEngine::kParamCount;++i)if(!r.readDouble(v[(size_t)i]))throw 15;
 return v;
}
bool eq(double a,double b){return std::abs(a-b)<=1e-12;}
bool check(int count){
 const auto v=loadState(count);
 const double legacy=0.37;
 bool ok=true;
 for(int i=0;i<std::min(count,(int)MixEngine::kParamCount);++i){
  double expected=0.05+0.025*i;
  if(i==MixEngine::kParamConsoleNoise)expected=legacy;
  if(i==MixEngine::kParamTubeType)expected=0.72;
  if(i==MixEngine::kParamMeterSource)expected=0.0;
  if(i==MixEngine::kParamTapeHiss)expected=0.66;
  if(i==MixEngine::kParamVinylNoise)expected=0.44;
  if(!eq(v[(size_t)i],expected))ok=false;
 }
 if(count<=MixEngine::kParamTubeType && !eq(v[MixEngine::kParamTubeType],0.5))ok=false;
 if(count<=MixEngine::kParamMeterSource && !eq(v[MixEngine::kParamMeterSource],1.0))ok=false;
 if(count<=MixEngine::kParamTapeHiss && !eq(v[MixEngine::kParamTapeHiss],legacy))ok=false;
 if(count<=MixEngine::kParamVinylNoise && !eq(v[MixEngine::kParamVinylNoise],legacy))ok=false;
 std::cout<<"legacyCount="<<count
          <<" tube="<<v[MixEngine::kParamTubeType]
          <<" meter="<<v[MixEngine::kParamMeterSource]
          <<" hiss="<<v[MixEngine::kParamTapeHiss]
          <<" surface="<<v[MixEngine::kParamVinylNoise]
          <<" "<<(ok?"PASS":"FAIL")<<"\n";
 return ok;
}
}
int main(){try{
 bool ok=true;
 for(int count:{26,27,28,29})ok=check(count)&&ok;
 std::cout<<(ok?"PASS":"FAIL")<<": V2/intermediate state migration matrix\n";
 return ok?0:1;
}catch(int c){std::cerr<<"State migration setup FAIL "<<c<<"\n";return c;}catch(...){return 90;}}
