#pragma once

#include <algorithm>
#include <cmath>

namespace MixEngine {

// Shared M/S stereo stage used by both the normal VST3 path and Studio One Mix FX.
// Keeping this in one place prevents the two processing paths from drifting apart.
struct StereoFieldState {
    double lowSideMemory = 0.0;
    double depthSideMemory = 0.0;
};

inline double stereoOnePoleCoefficient(double frequency, double sampleRate) noexcept {
    const double sr = sampleRate > 1.0 ? sampleRate : 44100.0;
    const double hz = std::clamp(frequency, 1.0, sr * 0.45);
    constexpr double kPi = 3.14159265358979323846;
    return 1.0 - std::exp(-2.0 * kPi * hz / sr);
}

// DEPTH is bipolar. Positive values move the upper stereo information back by
// attenuating it; negative values bring it forward. End stops are symmetric
// +/-4 dB so the full range is clearly audible while 0 remains exactly neutral.
inline double stereoDepthGain(double depthBipolar) noexcept {
    const double d = std::clamp(depthBipolar, -1.0, 1.0);
    return std::pow(10.0, (-4.0 * d) / 20.0);
}

inline void processStereoFieldSample(double& left,
                                     double& right,
                                     StereoFieldState& state,
                                     double widthGain,
                                     double lowMono,
                                     double lowMonoCoeff,
                                     double depthGain,
                                     double depthCoeff) noexcept {
    const double mid = 0.5 * (left + right);
    double side = 0.5 * (left - right) * std::max(0.0, widthGain);

    // Continuously remove the low-frequency side component up to 120 Hz.
    state.lowSideMemory += lowMonoCoeff * (side - state.lowSideMemory);
    side -= state.lowSideMemory * std::clamp(lowMono, 0.0, 1.0);

    // Split the remaining side signal around ~2 kHz and apply DEPTH only to
    // the upper side band. At depthGain == 1 the reconstruction is neutral.
    state.depthSideMemory += depthCoeff * (side - state.depthSideMemory);
    const double highSide = side - state.depthSideMemory;
    side = state.depthSideMemory + highSide * depthGain;

    left = mid + side;
    right = mid - side;
}

} // namespace MixEngine
