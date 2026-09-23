#pragma once

#include "nonlinear_cores.h"

#include <algorithm>
#include <array>
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

inline double analogLerp(double a, double b, double t) noexcept {
    const double m = std::clamp(t, 0.0, 1.0);
    return a + (b - a) * m;
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
            return {620.0, 1.48, 1.10, 0.024, 0.022, 0.035,
                    0.14, 4.5, 145.0, 72.0, 0.10};
        case 1: // 12AT7-inspired: balanced density and dynamic movement
            return {820.0, 1.88, 1.22, 0.044, 0.036, 0.065,
                    0.22, 3.2, 118.0, 56.0, 0.17};
        default: // 12AX7-inspired: earlier curvature and stronger bias/sag
            return {1080.0, 2.38, 1.38, 0.072, 0.052, 0.105,
                    0.32, 2.2, 92.0, 42.0, 0.26};
    }
}

inline TubeVoiceModel blendTubeVoiceModel(int fromType,
                                          int toType,
                                          double mix) noexcept {
    const TubeVoiceModel a = tubeVoiceModel(fromType);
    const TubeVoiceModel b = tubeVoiceModel(toType);
    const double t = std::clamp(mix, 0.0, 1.0);
    return {
        analogLerp(a.splitHz, b.splitHz, t),
        analogLerp(a.baseDrive, b.baseDrive, t),
        analogLerp(a.highDrive, b.highDrive, t),
        analogLerp(a.staticBias, b.staticBias, t),
        analogLerp(a.dynamicBias, b.dynamicBias, t),
        analogLerp(a.asymmetry, b.asymmetry, t),
        analogLerp(a.sag, b.sag, t),
        analogLerp(a.attackMs, b.attackMs, t),
        analogLerp(a.releaseMs, b.releaseMs, t),
        analogLerp(a.cathodeMs, b.cathodeMs, t),
        analogLerp(a.secondStage, b.secondStage, t)
    };
}

inline TubeVoiceModel weightedTubeVoiceModel(
    const std::array<double,3>& weights) noexcept {
    const TubeVoiceModel a = tubeVoiceModel(0);
    const TubeVoiceModel b = tubeVoiceModel(1);
    const TubeVoiceModel d = tubeVoiceModel(2);
    const double w0 = std::clamp(weights[0], 0.0, 1.0);
    const double w1 = std::clamp(weights[1], 0.0, 1.0);
    const double w2 = std::clamp(weights[2], 0.0, 1.0);
    const double sum = std::max(1.0e-12, w0 + w1 + w2);
    const auto mix = [&](double x0, double x1, double x2) noexcept {
        return (w0 * x0 + w1 * x1 + w2 * x2) / sum;
    };
    return {
        mix(a.splitHz,b.splitHz,d.splitHz),
        mix(a.baseDrive,b.baseDrive,d.baseDrive),
        mix(a.highDrive,b.highDrive,d.highDrive),
        mix(a.staticBias,b.staticBias,d.staticBias),
        mix(a.dynamicBias,b.dynamicBias,d.dynamicBias),
        mix(a.asymmetry,b.asymmetry,d.asymmetry),
        mix(a.sag,b.sag,d.sag),
        mix(a.attackMs,b.attackMs,d.attackMs),
        mix(a.releaseMs,b.releaseMs,d.releaseMs),
        mix(a.cathodeMs,b.cathodeMs,d.cathodeMs),
        mix(a.secondStage,b.secondStage,d.secondStage)
    };
}

// Stateful tube-colour model for the V2 MixEngine. It is a deliberately
// bounded gray-box model rather than a claim of component-for-component tube
// circuit emulation. The important analogue behaviours are represented:
// frequency-dependent drive, slow operating-point shift and program-dependent
// gain compression. The nonlinear part is expected to run inside the existing
// oversampling island.
inline double processTubeModelV2(double x,
                                 TubeModelState& state,
                                 const TubeVoiceModel& p,
                                 double amount,
                                 double sampleRate) noexcept {
    const double a = std::clamp(amount, 0.0, 1.0);
    const double c = analogCharacterAmount(a);

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
    // Do not hard-limit the final parallel result: the nonlinear branch is
    // already bounded, and clamping here would also clip the retained dry
    // transient at low Amount settings.
    return y;
}

inline double processTubeModelV2(double x,
                                 TubeModelState& state,
                                 int type,
                                 double amount,
                                 double sampleRate) noexcept {
    return processTubeModelV2(
        x, state, tubeVoiceModel(type), amount, sampleRate);
}


struct TapeMagneticState {
    double magnetisation = 0.0;
    double previousField = 0.0;
    double previousDirection = 1.0;
    double evenDcMemory = 0.0;

    void reset() noexcept {
        magnetisation = 0.0;
        previousField = 0.0;
        previousDirection = 1.0;
        evenDcMemory = 0.0;
    }
};

struct TapeSpeedModel {
    double driveBase;
    double driveRange;
    double coercivity;
    double feedback;
    double memoryMix;
    double saturation;
    double asymmetry;
};

inline TapeSpeedModel tapeSpeedModel(int speed) noexcept {
    switch (speed) {
        case 0: // 7.5 ips: earlier saturation, broader loop, strongest memory
            return {1.28, 2.15, 0.070, 0.22, 0.145, 1.08, 0.018};
        case 1: // 15 ips: balanced studio operating point
            return {1.12, 1.45, 0.045, 0.13, 0.082, 1.20, 0.012};
        default: // 30 ips: higher headroom, tighter and more open
            return {1.03, 0.85, 0.025, 0.075, 0.042, 1.35, 0.006};
    }
}

inline TapeSpeedModel blendTapeSpeedModel(int fromSpeed,
                                          int toSpeed,
                                          double mix) noexcept {
    const TapeSpeedModel a = tapeSpeedModel(fromSpeed);
    const TapeSpeedModel b = tapeSpeedModel(toSpeed);
    const double t = std::clamp(mix, 0.0, 1.0);
    return {
        analogLerp(a.driveBase, b.driveBase, t),
        analogLerp(a.driveRange, b.driveRange, t),
        analogLerp(a.coercivity, b.coercivity, t),
        analogLerp(a.feedback, b.feedback, t),
        analogLerp(a.memoryMix, b.memoryMix, t),
        analogLerp(a.saturation, b.saturation, t),
        analogLerp(a.asymmetry, b.asymmetry, t)
    };
}

struct TapePathModel {
    TapeSpeedModel magnetic{};
    double cutoff = 17800.0;
    double bumpFreq = 82.0;
    double bumpAmount = 0.060;
    double compressionStrength = 0.92;
    double wowHz = 0.48;
    double flutterHz = 5.8;
    double hissCorner = 1200.0;
    double hissTilt = 0.78;
    double hissLevel = 1.0;
};

inline TapePathModel tapePathModel(int speed) noexcept {
    switch (std::clamp(speed,0,2)) {
        case 0:
            return {tapeSpeedModel(0),12500.0,55.0,0.115,1.32,
                    0.38,4.7,850.0,0.82,0.82};
        case 1:
            return {tapeSpeedModel(1),17800.0,82.0,0.060,0.92,
                    0.48,5.8,1200.0,0.78,1.0};
        default:
            return {tapeSpeedModel(2),22500.0,125.0,0.020,0.58,
                    0.58,7.0,1750.0,0.72,1.10};
    }
}

inline TapePathModel weightedTapePathModel(
    const std::array<double,3>& weights) noexcept {
    const TapePathModel a = tapePathModel(0);
    const TapePathModel b = tapePathModel(1);
    const TapePathModel d = tapePathModel(2);
    const double w0 = std::clamp(weights[0], 0.0, 1.0);
    const double w1 = std::clamp(weights[1], 0.0, 1.0);
    const double w2 = std::clamp(weights[2], 0.0, 1.0);
    const double sum = std::max(1.0e-12, w0 + w1 + w2);
    const auto mix = [&](double x0, double x1, double x2) noexcept {
        return (w0 * x0 + w1 * x1 + w2 * x2) / sum;
    };
    TapePathModel out;
    out.magnetic = {
        mix(a.magnetic.driveBase,b.magnetic.driveBase,d.magnetic.driveBase),
        mix(a.magnetic.driveRange,b.magnetic.driveRange,d.magnetic.driveRange),
        mix(a.magnetic.coercivity,b.magnetic.coercivity,d.magnetic.coercivity),
        mix(a.magnetic.feedback,b.magnetic.feedback,d.magnetic.feedback),
        mix(a.magnetic.memoryMix,b.magnetic.memoryMix,d.magnetic.memoryMix),
        mix(a.magnetic.saturation,b.magnetic.saturation,d.magnetic.saturation),
        mix(a.magnetic.asymmetry,b.magnetic.asymmetry,d.magnetic.asymmetry)
    };
    out.cutoff = mix(a.cutoff,b.cutoff,d.cutoff);
    out.bumpFreq = mix(a.bumpFreq,b.bumpFreq,d.bumpFreq);
    out.bumpAmount = mix(a.bumpAmount,b.bumpAmount,d.bumpAmount);
    out.compressionStrength =
        mix(a.compressionStrength,b.compressionStrength,d.compressionStrength);
    out.wowHz = mix(a.wowHz,b.wowHz,d.wowHz);
    out.flutterHz = mix(a.flutterHz,b.flutterHz,d.flutterHz);
    out.hissCorner = mix(a.hissCorner,b.hissCorner,d.hissCorner);
    out.hissTilt = mix(a.hissTilt,b.hissTilt,d.hissTilt);
    out.hissLevel = mix(a.hissLevel,b.hissLevel,d.hissLevel);
    return out;
}

// Bounded hysteretic magnetic core. This is intentionally a stable gray-box
// approximation of the history dependence described by magnetic tape models,
// not a claim of solving the full Jiles-Atherton ODE. The static saturation
// path carries the main signal; the stateful magnetisation contributes a
// smaller path-dependent residual so the model remains usable at 1x/2x/4x.
inline double processTapeMagneticV2(double x,
                                    TapeMagneticState& state,
                                    const TapeSpeedModel& p,
                                    double amount,
                                    double sampleRate) noexcept {
    const double a = std::clamp(amount, 0.0, 1.0);
    const double c = analogCharacterAmount(a);

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
    double magnetic =
        normalized + p.memoryMix * c * hysteresisResidual;

    // Small AC-coupled even component represents record/playback electronics
    // and operating-point imperfections around the magnetic path. It is
    // intentionally restrained: the hysteretic/odd structure remains dominant.
    const double field2 = field * field;
    const double rawEven = field2 / (1.0 + 0.90 * field2);
    const double evenDcCoeff = analogOnePoleHz(5.0, sampleRate);
    state.evenDcMemory +=
        evenDcCoeff * (rawEven - state.evenDcMemory);
    magnetic += p.asymmetry * c * (rawEven - state.evenDcMemory);

    state.previousField = field;

    // Parallel mixing is applied by the processor after oversampling so the
    // dry path can be latency-aligned exactly. Return the fully processed
    // magnetic branch here.
    return std::clamp(magnetic, -4.0, 4.0);
}

inline double processTapeMagneticV2(double x,
                                    TapeMagneticState& state,
                                    int speed,
                                    double amount,
                                    double sampleRate) noexcept {
    return processTapeMagneticV2(
        x, state, tapeSpeedModel(speed), amount, sampleRate);
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
