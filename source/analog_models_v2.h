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
    double evenDcMemory = 0.0;

    void reset() noexcept {
        lowMemory = 0.0;
        envelope = 0.0;
        biasMemory = 0.0;
        cathodeMemory = 0.0;
        evenDcMemory = 0.0;
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
    // Divide by the explicit drive so low-level gain stays close to unity.
    // The drive then changes curvature/headroom instead of acting as a hidden
    // make-up gain. Sag is intentionally not divided out: it remains dynamic
    // compression.
    const double first =
        (std::atan(z + bias) - atanBias) *
        (1.0 + bias * bias) /
        std::max(1.0e-9, drive);

    // A true even-symmetry component generates H2. Its DC component is removed
    // with a very slow local memory, analogous to AC coupling around a tube
    // stage, so asymmetry does not inject a persistent DC offset.
    const double z2 = z * z;
    const double rawEven = z2 / (1.0 + 0.85 * z2);
    const double evenDcCoeff = analogOnePoleHz(6.0, sampleRate);
    state.evenDcMemory +=
        evenDcCoeff * (rawEven - state.evenDcMemory);
    const double even =
        p.asymmetry * c * (rawEven - state.evenDcMemory);

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


struct TapeMagneticState {
    double magnetisation = 0.0;
    double previousField = 0.0;
    double previousDirection = 1.0;

    void reset() noexcept {
        magnetisation = 0.0;
        previousField = 0.0;
        previousDirection = 1.0;
    }
};

struct TapeSpeedModel {
    double driveBase;
    double driveRange;
    double coercivity;
    double feedback;
    double memoryMix;
    double saturation;
};

inline TapeSpeedModel tapeSpeedModel(int speed) noexcept {
    switch (speed) {
        case 0: // 7.5 ips: earlier saturation, strongest hysteretic colour
            return {1.20, 1.55, 0.055, 0.16, 0.105, 1.28};
        case 1: // 15 ips: balanced studio operating point
            return {1.12, 1.35, 0.045, 0.13, 0.082, 1.18};
        default: // 30 ips: cleaner, more open magnetic path
            return {1.06, 1.10, 0.034, 0.10, 0.060, 1.10};
    }
}

// Bounded hysteretic magnetic core. This is intentionally a stable gray-box
// approximation of the history dependence described by magnetic tape models,
// not a claim of solving the full Jiles-Atherton ODE. The static saturation
// path carries the main signal; the stateful magnetisation contributes a
// smaller path-dependent residual so the model remains usable at 1x/2x/4x.
inline double processTapeMagneticV2(double x,
                                    TapeMagneticState& state,
                                    int speed,
                                    double amount,
                                    double sampleRate) noexcept {
    const double a = std::clamp(amount, 0.0, 1.0);
    const double c = analogCharacterAmount(a);
    const TapeSpeedModel p = tapeSpeedModel(speed);

    const double drive = p.driveBase + p.driveRange * c;
    const double field = x * drive;
    const double delta = field - state.previousField;
    if (std::abs(delta) > 1.0e-12)
        state.previousDirection = delta > 0.0 ? 1.0 : -1.0;

    // Coercive shift plus mean-field feedback forms a compact hysteresis loop.
    // Updating more strongly on larger field movement avoids turning the
    // magnetisation state into an ordinary audio low-pass filter.
    const double shifted =
        field + p.feedback * c * state.magnetisation -
        p.coercivity * c * state.previousDirection;
    const double target = std::tanh(shifted / p.saturation);
    // Normalise state evolution to real time so changing oversampling quality
    // does not change the tape's magnetic personality. At 48 kHz this retains
    // the intended response; at 2x/4x each substep advances proportionally less.
    const double srScale =
        std::clamp(48000.0 / std::max(1.0, sampleRate), 0.125, 2.0);
    const double movement48 =
        std::clamp(0.10 + 1.75 * std::abs(delta), 0.10, 0.92);
    const double movement =
        1.0 - std::pow(1.0 - movement48, srScale);
    state.magnetisation +=
        movement * (target - state.magnetisation);

    // Main bounded saturation law plus a modest path-dependent residual.
    const double k = 0.42 + 1.18 * c;
    const double staticMag =
        field / std::sqrt(1.0 + k * field * field);
    const double reference =
        std::tanh(field / p.saturation);
    const double hysteresisResidual =
        state.magnetisation - reference;

    // Normalize the static branch by its actual excitation drive. This keeps
    // the low-level transfer near unity while allowing the same drive to move
    // the magnetic path deeper into saturation at higher levels.
    const double normalized =
        staticMag / std::max(1.0e-9, drive);
    const double magnetic =
        normalized + p.memoryMix * c * hysteresisResidual;

    state.previousField = field;

    // Parallel mixing is applied by the processor after oversampling so the
    // dry path can be latency-aligned exactly. Return the fully processed
    // magnetic branch here.
    return std::clamp(magnetic, -4.0, 4.0);
}


inline double processVinylGrooveV2(double x,
                                   double character,
                                   double wear) noexcept {
    const double c = std::clamp(character, 0.0, 1.0);
    const double w = std::clamp(wear, 0.0, 1.0);
    const double drive = 1.0 + 0.42 * c + 0.58 * w;
    const double bias = 0.010 * c + 0.020 * w;
    const double z = x * drive;
    const double centre = std::atan(bias);
    // Normalize the explicit drive so COLOR/WEAR change curvature rather than
    // quietly raising low-level gain.
    const double curved =
        (std::atan(z + bias) - centre) * (1.0 + bias * bias) /
        std::max(1.0e-9, drive);

    const double z2 = z * z;
    // True even-symmetry tracing term. The processor AC-couples the processed
    // groove branch after oversampling so this can generate realistic H2
    // without leaking its DC component to the plugin output.
    const double tracingAsym =
        (0.012 * c + 0.026 * w) *
        z2 / (1.0 + 0.90 * z2);
    const double groove = curved + tracingAsym;
    return std::clamp(groove, -4.0, 4.0);
}

} // namespace MixEngine
