#pragma once
#include "pluginids.h"
#include "pluginterfaces/base/ibstream.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace MixEngine {
// Released ABI: 26, 27, 28, 29 or 30 little-endian doubles. Only an empty
// optional slot means legacy EOF; a partially read double is corruption.
inline bool readParameterState(Steinberg::IBStream* stream,
                              const std::array<double,kParamCount>& defaults,
                              std::array<double,kParamCount>& result) {
 if(!stream)return false;
 auto candidate=defaults;
 for(Steinberg::Vst::ParamID id=0;id<kParamCount;++id){
  unsigned char bytes[8]{};Steinberg::int32 count=0;
  const auto status=stream->read(bytes,8,&count);
  if(count==0&&id>=kParamTubeType&&(status==Steinberg::kResultOk||status==Steinberg::kResultFalse)){
   if(id<=kParamTubeType)candidate[kParamTubeType]=.5;
   if(id<=kParamMeterSource)candidate[kParamMeterSource]=1.;
   if(id<=kParamTapeHiss)candidate[kParamTapeHiss]=candidate[kParamConsoleNoise];
   candidate[kParamVinylNoise]=candidate[kParamConsoleNoise];
   result=candidate;return true;
  }
  if(status!=Steinberg::kResultOk||count!=8)return false;
  std::uint64_t bits=0;for(int i=0;i<8;++i)bits|=std::uint64_t(bytes[i])<<(8*i);
  double value=0;std::memcpy(&value,&bits,sizeof(value));
  candidate[id]=std::isfinite(value)?std::clamp(value,0.,1.):defaults[id];
 }
 result=candidate;return true;
}
}
