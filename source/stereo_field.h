#pragma once

#include <algorithm>
#include <cmath>

namespace MixEngine {

// Shared M/S stereo stage used by both the normal VST3 path and Studio One Mix FX.
// Keeping this in one place prevents the two processing paths from drifting apart.
struct StereoFieldState {
    double lowSideMemory = 0.0;
    double depthSideMemory = 0.0;
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;
    double crossEnergy = 0.0;
    double midEnergy = 0.0;
    double sideEnergy = 0.0;
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
                                     double depthCoeff,
                                     double correlationCoeff = 0.001) noexcept {
    const double inL = left;
    const double inR = right;
    const double mid = 0.5 * (inL + inR);
    const double rawSide = 0.5 * (inL - inR);

    // Slow signal statistics are used only as a safety/intelligence layer when
    // the user asks for WIDTH > 100%. Neutral/narrow settings remain purely
    // deterministic M/S processing.
    const double ec = std::clamp(correlationCoeff, 1.0e-7, 1.0);
    state.leftEnergy += ec * (inL * inL - state.leftEnergy);
    state.rightEnergy += ec * (inR * inR - state.rightEnergy);
    state.crossEnergy += ec * (inL * inR - state.crossEnergy);
    state.midEnergy += ec * (mid * mid - state.midEnergy);
    state.sideEnergy += ec * (rawSide * rawSide - state.sideEnergy);

    double effectiveWidth = std::max(0.0, widthGain);
    if (effectiveWidth > 1.0) {
        const double denom =
            std::sqrt(std::max(1.0e-18,
                               state.leftEnergy * state.rightEnergy));
        const double corr =
            denom > 0.0
                ? std::clamp(state.crossEnergy / denom, -1.0, 1.0)
                : 1.0;
        const double sideRatio =
            std::sqrt(std::max(0.0, state.sideEnergy)) /
            std::max(1.0e-9,
                     std::sqrt(std::max(0.0, state.midEnergy)));

        const double antiPhaseRisk =
            std::clamp(-corr, 0.0, 1.0);
        const double sideHeavyRisk =
            std::clamp((sideRatio - 1.0) / 2.0, 0.0, 1.0);
        const double risk =
            std::max(antiPhaseRisk, sideHeavyRisk);

        // Never cancel the user's widening request; only reduce the excess
        // width progressively as phase risk grows.
        const double extra = effectiveWidth - 1.0;
        effectiveWidth =
            1.0 + extra * (1.0 - 0.68 * risk);
    }

    double side = rawSide * effectiveWidth;

    // Continuously remove the low-frequency side component up to 120 Hz.
    state.lowSideMemory +=
        lowMonoCoeff * (side - state.lowSideMemory);
    side -=
        state.lowSideMemory *
        std::clamp(lowMono, 0.0, 1.0);

    // DEPTH affects only upper-side information. It does not introduce a
    // left/right sample delay, preserving the mono timing relationship.
    state.depthSideMemory +=
        depthCoeff * (side - state.depthSideMemory);
    const double highSide =
        side - state.depthSideMemory;
    side =
        state.depthSideMemory +
        highSide * depthGain;

    left = mid + side;
    right = mid - side;
}

} // namespace MixEngine
