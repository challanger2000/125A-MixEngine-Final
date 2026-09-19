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

    double gain = 1.8;
    double bias = 0.035;
    double asym = 0.025;
    switch (type) {
        case 0: gain = 1.55; bias = 0.025; asym = 0.016; break;
        case 1: gain = 1.95; bias = 0.045; asym = 0.028; break;
        default: gain = 2.45; bias = 0.070; asym = 0.045; break;
    }

    const double driven = x * (1.0 + (gain - 1.0) * a);
    const double b = bias * a;
    const double centered = std::tanh(b);
    const double slope = std::max(1.0e-9, 1.0 - centered * centered);
    double shaped = (std::tanh(driven + b) - centered) / slope;
    shaped += asym * a * driven * std::abs(driven);
    const double wet = 0.25 + 0.75 * a;
    return x + (shaped - x) * wet * a;
}

} // namespace MixEngine
