#pragma once
#include <array>
#include <cstddef>
namespace MixEngine {
// IParamValueQueue represents piecewise LINEAR curves. The preceding block's
// last value is the implicit point at offset -1. Actual jumps use adjacent
// old/new points, not one isolated endpoint at the jump's destination sample.
inline double linearAutomationValue(int previousOffset,double previousValue,
                                    int nextOffset,double nextValue,int sample) noexcept {
 if(sample<=previousOffset)return previousValue;
 if(sample>=nextOffset||nextOffset<=previousOffset)return nextValue;
 return previousValue+(nextValue-previousValue)*
        (double(sample-previousOffset)/double(nextOffset-previousOffset));
}
template<std::size_t N,class Lanes>
bool applyAutomationLanes(std::array<double,N>& values,const std::array<double,N>& start,
                          const Lanes& lanes,std::array<std::size_t,N>& cursors,int sample) noexcept {
 bool changed=false;
 for(std::size_t id=0;id<N;++id){
  const auto& lane=lanes[id];if(lane.empty())continue;
  auto& cursor=cursors[id];while(cursor<lane.size()&&lane[cursor].offset<=sample)++cursor;
  const int previousOffset=cursor?lane[cursor-1].offset:-1;
  const double previousValue=cursor?lane[cursor-1].value:start[id];
  const double value=cursor<lane.size()
      ?linearAutomationValue(previousOffset,previousValue,lane[cursor].offset,lane[cursor].value,sample)
      :previousValue;
  if(value!=values[id]){values[id]=value;changed=true;}
 }
 return changed;
}
}
