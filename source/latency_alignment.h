#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
namespace MixEngine {
constexpr int kFixedLatencySamples=21; // V2 oversampling budget, retained for legacy diagnostics.
constexpr int kV3MaxLatencySamples=64;
inline int v3TapeNominalDelaySamples(double sampleRate) noexcept {
 return std::max(0,static_cast<int>(std::ceil(0.00015*std::max(1.0,sampleRate))));
}
// readTransport() writes the current sample first, then performs a four-point
// cubic read around floor(write-delay). The dominant impulse tap therefore
// lands two samples earlier than ceil(centerDelaySamples). Keep the conservative
// nominal value for the fixed host budget, but compensate the live path using
// the actual cubic-interpolator peak position.
inline int v3TapePeakDelaySamples(double sampleRate) noexcept {
 return std::max(2,v3TapeNominalDelaySamples(sampleRate)-2);
}
inline int v3ReportedLatencySamples(double sampleRate) noexcept {
 return kFixedLatencySamples+v3TapeNominalDelaySamples(sampleRate);
}
inline int oversamplingBulkDelay(int factor,int islands) noexcept {
 if(factor<=1||islands<=0)return 0; islands=std::clamp(islands,0,4);
 static constexpr int d2[5]={0,5,9,12,16}; static constexpr int d4[5]={0,6,11,16,21};
 return factor>=4?d4[islands]:d2[islands];
}
inline int latencyCompensation(int factor,int islands) noexcept {return kFixedLatencySamples-oversamplingBulkDelay(factor,islands);}
inline int v3LatencyCompensation(double sampleRate,int factor,int islands,bool tapeActive) noexcept {
 const int internal=oversamplingBulkDelay(factor,islands)+(tapeActive?v3TapePeakDelaySamples(sampleRate):0);
 return std::clamp(v3ReportedLatencySamples(sampleRate)-internal,0,kV3MaxLatencySamples);
}
class LatencyAligner {
public:
 void reset() noexcept {buffer_.fill(0.0);write_=0;}
 double process(double x,int delay) noexcept {delay=std::clamp(delay,0,kV3MaxLatencySamples);buffer_[write_]=x;const std::size_t size=buffer_.size();const std::size_t read=(write_+size-static_cast<std::size_t>(delay))%size;const double y=buffer_[read];write_=(write_+1)%size;return y;}
private:
 std::array<double,kV3MaxLatencySamples+1> buffer_{}; std::size_t write_=0;
};
}
