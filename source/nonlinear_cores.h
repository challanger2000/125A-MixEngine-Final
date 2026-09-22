#pragma once

#include <algorithm>
#include <cmath>

namespace MixEngine {

inline double analogCharacterAmount(double amount) noexcept {
    const double a = std::clamp(amount, 0.0, 1.0);
    // A switched-on analogue stage is never perfectly invisible: keep a very
    // small base character at 0%, then use a gentle concave curve so the lower
    // third is useful while 100% still reaches the full model.
    return 0.06 + 0.94 * std::pow(a, 0.85);
}


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
    const double character = analogCharacterAmount(a);

    double drive = 1.75;
    double bias = 0.030;
    double asym = 0.020;
    double secondStage = 0.18;
    switch (type) {
        case 0: drive = 1.45; bias = 0.020; asym = 0.012; secondStage = 0.12; break;
        case 1: drive = 1.85; bias = 0.038; asym = 0.022; secondStage = 0.18; break;
        default: drive = 2.30; bias = 0.060; asym = 0.036; secondStage = 0.25; break;
    }

    const double driven = x * (1.0 + (drive - 1.0) * character);
    const double b = bias * character;
    const double centered = std::tanh(b);
    const double slope = std::max(1.0e-9, 1.0 - centered * centered);

    double first = (std::tanh(driven + b) - centered) / slope;

    // Bounded even-harmonic term: contributes tube asymmetry without polynomial
    // runaway on deliberately hot internal levels.
    const double d2 = driven * driven;
    first += asym * character * (driven * std::abs(driven)) / (1.0 + 0.65 * d2);

    // Gentle secondary curvature adds density without replacing the transient.
    const double stage2Drive = 1.0 + secondStage * character;
    const double second = std::tanh(first * stage2Drive) / stage2Drive;
    const double dense = first + (second - first) * (0.15 + 0.35 * character);

    // At 0% Amount the enabled stage contributes only 4% processed signal.
    // The control then opens smoothly to 90% wet at 100%, leaving a small dry
    // path even at the most aggressive setting.
    const double wet = 0.04 + 0.86 * std::pow(a, 0.90);
    return x + (dense - x) * wet;
}

} // namespace MixEngine
