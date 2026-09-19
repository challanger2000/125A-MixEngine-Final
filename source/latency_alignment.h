#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
namespace MixEngine {
constexpr int kFixedLatencySamples=21;
inline int oversamplingBulkDelay(int factor,int islands) noexcept {
 if(factor<=1||islands<=0)return 0; islands=std::clamp(islands,0,4);
 static constexpr int d2[5]={0,5,9,12,16}; static constexpr int d4[5]={0,6,11,16,21};
 return factor>=4?d4[islands]:d2[islands];
}
inline int latencyCompensation(int factor,int islands) noexcept {return kFixedLatencySamples-oversamplingBulkDelay(factor,islands);}
class LatencyAligner {
public:
 void reset() noexcept {buffer_.fill(0.0);write_=0;}
 double process(double x,int delay) noexcept {delay=std::clamp(delay,0,kFixedLatencySamples);buffer_[write_]=x;const std::size_t size=buffer_.size();const std::size_t read=(write_+size-static_cast<std::size_t>(delay))%size;const double y=buffer_[read];write_=(write_+1)%size;return y;}
private:
 std::array<double,kFixedLatencySamples+1> buffer_{}; std::size_t write_=0;
};
}
