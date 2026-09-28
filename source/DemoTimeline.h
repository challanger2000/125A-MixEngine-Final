#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace MixEngine {
class DemoTimeline {
public:
 void configure(double sr,bool licensed) noexcept {licensed_=licensed;sr_=std::isfinite(sr)&&sr>1.0?sr:44100.0;normal_=toS(60.0);mute_=toS(3.0);fade_=std::max<std::int64_t>(1,toS(0.010));phase_=0;}
 bool licensed() const noexcept{return licensed_;}
 std::int64_t phase() const noexcept{return phase_;}
 void advance(int samples) noexcept {if(licensed_||samples<=0)return;const auto cycle=normal_+mute_;phase_=(phase_+static_cast<std::int64_t>(samples))%cycle;}
 template<class Sample>void processAndAdvance(Sample**o,int ch,int n) noexcept {if(licensed_||!o||ch<=0||n<=0)return;processAtPhase(o,ch,n,phase_);advance(n);}
 template<class Sample>void processAtPhase(Sample**o,int ch,int n,std::int64_t start) const noexcept {if(licensed_||!o||ch<=0||n<=0)return;const auto cycle=normal_+mute_;auto p=start%cycle;for(int i=0;i<n;++i){const double g=gain(p);for(int c=0;c<ch;++c)if(o[c])o[c][i]=static_cast<Sample>(static_cast<double>(o[c][i])*g);if(++p>=cycle)p=0;}}
private:
 std::int64_t toS(double s)const noexcept{return std::max<std::int64_t>(1,static_cast<std::int64_t>(std::llround(sr_*s)));}
 double gain(std::int64_t p)const noexcept{if(p<normal_)return 1.0;const auto m=p-normal_;if(m<fade_)return 1.0-static_cast<double>(m+1)/fade_;const auto fs=std::max<std::int64_t>(fade_,mute_-fade_);if(m<fs)return 0.0;if(m<mute_)return std::clamp(static_cast<double>(m-fs+1)/std::max<std::int64_t>(1,mute_-fs),0.0,1.0);return 1.0;}
 bool licensed_=false;double sr_=44100.0;std::int64_t normal_=0,mute_=0,fade_=1,phase_=0;
};
}
