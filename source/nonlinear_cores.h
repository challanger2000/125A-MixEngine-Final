#pragma once

#include <algorithm>
#include <cmath>

namespace MixEngine {

inline double consoleSoftClip(double x, double amount) noexcept {
    const double a = std::clamp(amount, 0.0, 1.0);
    if (a <= 0.0)
        return x;

    // DRIVE=0 must be a true identity point. As DRIVE rises, progressively
    // blend in the normalized tanh transfer instead of starting fully inside
    // a tanh curve even at zero.
    const double shape = 1.0 + a;
    const double norm = std::tanh(shape);
    const double saturated = norm > 0.0 ? std::tanh(x * shape) / norm : x;
    return x + (saturated - x) * a;
}

// Exact nonlinear/shaping portion of the existing Console DSP.
// Host-rate tolerance/bias, low/high state calculation, DC blocking and noise
// intentionally remain outside this function.
inline double processConsoleNonlinearCore(double x,
                                          double low,
                                          double high,
                                          int mode,
                                          double drive) noexcept {
    const double d = std::clamp(drive, 0.0, 1.0);
    double y = x;

    switch (mode) {
        case 0:
            y = consoleSoftClip(x, 0.20 * d);
            break;
        case 1:
            y = consoleSoftClip(x + 0.035 * d * low, 0.65 * d);
            y += 0.008 * d * high * std::abs(x);
            break;
        case 2:
            y = consoleSoftClip(x + 0.070 * d * low
                                  + 0.018 * d * x * x * (x >= 0.0 ? 1.0 : -0.55), d);
            y -= 0.018 * d * high;
            break;
        default:
            y = consoleSoftClip(x + 0.012 * d * high, 0.45 * d);
            y += 0.010 * d * high * (1.0 - std::min(1.0, std::abs(x)));
            break;
    }

    return y;
}

// Exact stateless nonlinear Tube DSP. Tube type values intentionally match the
// existing processor convention: 0=12AU7, 1=12AT7, 2=12AX7.
inline double processTubeNonlinearCore(double x, int type, double amount) noexcept {
    const double a = std::clamp(amount, 0.0, 1.0);
    if (a <= 0.0)
        return x;

    // Keep a meaningful clean path at every setting. The tube stage is intended
    // to add density and asymmetric harmonics without flattening transients into
    // a fully-wet static waveshaper at maximum Amount.
    double drive = 1.75;
    double bias = 0.030;
    double asym = 0.020;
    double secondStage = 0.18;
    switch (type) {
        case 0: drive = 1.45; bias = 0.020; asym = 0.012; secondStage = 0.12; break;
        case 1: drive = 1.85; bias = 0.038; asym = 0.022; secondStage = 0.18; break;
        default: drive = 2.30; bias = 0.060; asym = 0.036; secondStage = 0.25; break;
    }

    const double driven = x * (1.0 + (drive - 1.0) * a);
    const double b = bias * a;
    const double centered = std::tanh(b);
    const double slope = std::max(1.0e-9, 1.0 - centered * centered);

    // First triode-like stage: asymmetric, DC-centred and small-signal
    // normalised so low-level material is not needlessly level-shifted.
    double first = (std::tanh(driven + b) - centered) / slope;

    // Bounded even-harmonic term. Unlike the old x*abs(x) term, this cannot
    // grow without bound at hot internal levels.
    const double d2 = driven * driven;
    first += asym * a * (driven * std::abs(driven)) / (1.0 + 0.65 * d2);

    // A gentle second stage adds density progressively. Its contribution is
    // deliberately parallel rather than replacing the first stage.
    const double stage2Drive = 1.0 + secondStage * a;
    const double second = std::tanh(first * stage2Drive) / stage2Drive;
    const double dense = first + (second - first) * (0.20 + 0.35 * a);

    // Parallel tube blend: retain at least 20% of the original transient path
    // even at maximum Amount.
    const double wet = a * (0.22 + 0.58 * a);
    return x + (dense - x) * wet;
}

} // namespace MixEngine
