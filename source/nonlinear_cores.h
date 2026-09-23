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
    if (a <= 0.0) return x;
    const double shape = 1.0 + 1.35 * a;
    const double saturated = std::tanh(x * shape) / shape;
    return x + (saturated - x) * a;
}

inline double consoleAtanClip(double x, double amount) noexcept {
    const double a = std::clamp(amount, 0.0, 1.0);
    if (a <= 0.0) return x;
    const double shape = 1.0 + 2.20 * a;
    const double saturated = std::atan(x * shape) / shape;
    return x + (saturated - x) * a;
}

inline double consoleSoftSign(double x, double amount) noexcept {
    const double a = std::clamp(amount, 0.0, 1.0);
    if (a <= 0.0) return x;
    const double k = 0.35 + 1.45 * a;
    const double saturated = x / (1.0 + k * std::abs(x));
    return x + (saturated - x) * a;
}

// V2 Console nonlinear families. DRIVE=0 is an exact identity point for every
// mode. The modes intentionally use different transfer families and spectral
// weighting rather than merely scaling one waveshaper.
inline double processConsoleNonlinearCore(double x,
                                          double low,
                                          double high,
                                          int mode,
                                          double drive) noexcept {
    const double d = std::clamp(drive, 0.0, 1.0);
    if (d <= 0.0) return x;

    switch (mode) {
        case 0: { // Clean: high headroom, mostly symmetric, slight HF density
            const double pre =
                x + 0.012 * d * high - 0.004 * d * low;
            return consoleAtanClip(pre, 0.24 * d);
        }

        case 1: { // Classic: broader saturation with controlled even harmonics
            const double pre =
                x + 0.030 * d * low + 0.010 * d * high;
            double y = consoleSoftClip(pre, 0.58 * d);
            const double p2 = pre * pre;
            y += 0.012 * d *
                 (pre * std::abs(pre)) /
                 (1.0 + 0.70 * p2);
            return y;
        }

        case 2: { // Vintage: softer knees, low-mid density, stronger asymmetry
            const double pre =
                x + 0.062 * d * low - 0.014 * d * high;
            double y = consoleSoftSign(pre, 0.72 * d);
            const double p2 = pre * pre;
            y += 0.022 * d *
                 (pre * std::abs(pre)) /
                 (1.0 + 0.55 * p2);
            // Mild second curvature stage creates density without hard clipping.
            const double second =
                std::tanh(y * (1.0 + 0.30 * d)) /
                (1.0 + 0.30 * d);
            return y + (second - y) * (0.16 + 0.20 * d);
        }

        default: { // Modern: tighter LF, faster/cleaner odd-harmonic structure
            const double pre =
                x - 0.018 * d * low + 0.026 * d * high;
            double y = consoleAtanClip(pre, 0.46 * d);
            const double p2 = pre * pre;
            y += 0.006 * d *
                 (pre * pre * pre) /
                 (1.0 + 0.85 * p2);
            return y;
        }
    }
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
