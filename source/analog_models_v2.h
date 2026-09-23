#pragma once

#include "nonlinear_cores.h"

#include <algorithm>
#include <cmath>

namespace MixEngine {

constexpr double kAnalogPi = 3.14159265358979323846;

inline double analogOnePoleHz(double hz, double sampleRate) noexcept {
    const double sr = sampleRate > 1.0 ? sampleRate : 44100.0;
    const double f = std::clamp(hz, 0.1, 0.45 * sr);
    return 1.0 - std::exp(-2.0 * kAnalogPi * f / sr);
}

inline double analogTimeCoeffMs(double ms, double sampleRate) noexcept {
    const double sr = sampleRate > 1.0 ? sampleRate : 44100.0;
    return std::exp(-1.0 / (0.001 * std::max(0.01, ms) * sr));
}

struct TubeModelState {
    double lowMemory = 0.0;
    double envelope = 0.0;
    double biasMemory = 0.0;
    double cathodeMemory = 0.0;

    void reset() noexcept {
        lowMemory = 0.0;
        envelope = 0.0;
        biasMemory = 0.0;
        cathodeMemory = 0.0;
    }
};

struct TubeVoiceModel {
    double splitHz;
    double baseDrive;
    double highDrive;
    double staticBias;
    double dynamicBias;
    double asymmetry;
    double sag;
    double attackMs;
    double releaseMs;
    double cathodeMs;
    double secondStage;
};

// The three voices intentionally differ in dynamics and spectral drive, not
// merely in the scalar amount of distortion.
inline TubeVoiceModel tubeVoiceModel(int type) noexcept {
    switch (type) {
        case 0: // 12AU7-inspired: broad, relatively clean, slower and softer
            return {620.0, 1.48, 1.10, 0.020, 0.020, 0.018,
                    0.14, 4.5, 145.0, 72.0, 0.10};
        case 1: // 12AT7-inspired: balanced density and dynamic movement
            return {820.0, 1.88, 1.22, 0.036, 0.032, 0.032,
                    0.22, 3.2, 118.0, 56.0, 0.17};
        default: // 12AX7-inspired: earlier curvature and stronger bias/sag
            return {1080.0, 2.38, 1.38, 0.060, 0.046, 0.050,
                    0.32, 2.2, 92.0, 42.0, 0.26};
    }
}

// Stateful tube-colour model for the V2 MixEngine. It is a deliberately
// bounded gray-box model rather than a claim of component-for-component tube
// circuit emulation. The important analogue behaviours are represented:
// frequency-dependent drive, slow operating-point shift and program-dependent
// gain compression. The nonlinear part is expected to run inside the existing
// oversampling island.
inline double processTubeModelV2(double x,
                                 TubeModelState& state,
                                 int type,
                                 double amount,
                                 double sampleRate) noexcept {
    const double a = std::clamp(amount, 0.0, 1.0);
    const double c = analogCharacterAmount(a);
    const TubeVoiceModel p = tubeVoiceModel(type);

    // Frequency-dependent excitation. High frequencies reach the nonlinear
    // transfer somewhat differently from the body of the signal, preventing
    // the whole spectrum from behaving like one static waveshaper.
    const double splitCoeff = analogOnePoleHz(p.splitHz, sampleRate);
    state.lowMemory += splitCoeff * (x - state.lowMemory);
    const double low = state.lowMemory;
    const double high = x - low;
    const double spectralInput =
        low * (1.0 + 0.12 * c) +
        high * (1.0 + (p.highDrive - 1.0) * c);

    // Fast/slow level memory creates gentle supply/cathode-like compression.
    const double level = std::abs(spectralInput);
    const double attack = analogTimeCoeffMs(p.attackMs, sampleRate);
    const double release = analogTimeCoeffMs(p.releaseMs, sampleRate);
    const double envCoeff = level > state.envelope ? attack : release;
    state.envelope = envCoeff * state.envelope + (1.0 - envCoeff) * level;

    const double cathodeCoeff = analogTimeCoeffMs(p.cathodeMs, sampleRate);
    state.cathodeMemory =
        cathodeCoeff * state.cathodeMemory +
        (1.0 - cathodeCoeff) * state.envelope;

    const double excess = std::max(0.0, state.cathodeMemory - 0.16);
    const double sagGain = 1.0 / (1.0 + p.sag * c * excess);

    // Slow signed energy memory moves the operating point. This is the key
    // distinction from a memoryless atan/tanh saturator and causes the harmonic
    // balance to evolve with programme material.
    const double biasCoeff = analogTimeCoeffMs(28.0 + 34.0 * (1.0 - c), sampleRate);
    const double signedEnergy =
        spectralInput * std::abs(spectralInput) /
        (1.0 + 0.75 * spectralInput * spectralInput);
    state.biasMemory =
        biasCoeff * state.biasMemory +
        (1.0 - biasCoeff) * signedEnergy;

    const double drive = 1.0 + (p.baseDrive - 1.0) * c;
    const double z = spectralInput * drive * sagGain;
    const double bias = c * (p.staticBias + p.dynamicBias * state.biasMemory);

    // Locally slope-normalised atan stage. The DC operating point is removed,
    // and the local small-signal slope is restored before the dynamic/spectral
    // behaviour is blended back with the dry signal.
    const double atanBias = std::atan(bias);
    const double first =
        (std::atan(z + bias) - atanBias) * (1.0 + bias * bias);

    // Bounded asymmetric term controls even-harmonic growth without polynomial
    // runaway when the user deliberately drives the stage hard.
    const double z2 = z * z;
    const double even =
        p.asymmetry * c * (z * std::abs(z)) / (1.0 + 0.85 * z2);

    const double stage1 = first + even;
    const double secondDrive = 1.0 + p.secondStage * c;
    const double stage2 =
        std::tanh(stage1 * secondDrive) /
        std::max(1.0e-9, secondDrive);
    const double dense =
        stage1 + (stage2 - stage1) * (0.12 + 0.38 * c);

    // Keep the established V1 UX: enabled 0% still has a subtle hardware
    // fingerprint, while the top end is allowed to become deliberately strong.
    const double wet = 0.04 + 0.91 * std::pow(a, 0.88);
    const double y = x + (dense - x) * wet;
    return std::clamp(y, -4.0, 4.0);
}

} // namespace MixEngine
