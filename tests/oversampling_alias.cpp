#include "oversampling.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kSampleRate = 48000.0;
constexpr std::size_t kCount = 131072;
constexpr std::size_t kWarmup = 8192;

// Deliberately strong, memoryless saturation. The test tone is chosen so that
// odd harmonics fold back into the host band at 1x. Correct oversampling moves
// as much of that nonlinear spectrum as the selected internal sample rate can
// represent into the high-rate domain, where the downsampler can reject it.
double saturate(double x) {
    return std::tanh(4.0 * x);
}

struct Result {
    int factor{};
    double fundamental{};
    double aliasRms{};
    double aliasDbRelative{};
    bool finite{true};
};

Result runAliasTest(int factor) {
    // 18 kHz is exactly bin-centred for this record length after warmup because
    // 18000 / 48000 = 3/8. At 1x, the 3rd harmonic (54 kHz) aliases to 6 kHz.
    // This is also an intentionally severe 2x stress case: at 96 kHz internal
    // rate the 5th harmonic (90 kHz) folds to 6 kHz before the downsampler, so
    // a 2x path cannot remove all of the residue that a 4x path can avoid.
    constexpr double frequency = 18000.0;
    constexpr double amplitude = 0.72;

    MixEngine::OversamplingEngine os;
    std::vector<double> output(kCount);
    for (std::size_t n = 0; n < kCount; ++n) {
        const double x = amplitude * std::sin(2.0 * kPi * frequency * static_cast<double>(n) / kSampleRate);
        output[n] = os.process(x, factor, saturate);
    }

    Result r{};
    r.factor = factor;
    for (double x : output)
        r.finite = r.finite && std::isfinite(x);

    // Fit and remove the wanted 18 kHz fundamental. The remaining RMS is the
    // in-band nonlinear residue (principally aliases for this memoryless test).
    long double sinDot = 0.0;
    long double cosDot = 0.0;
    long double sin2 = 0.0;
    long double cos2 = 0.0;
    for (std::size_t n = kWarmup; n < kCount; ++n) {
        const double phase = 2.0 * kPi * frequency * static_cast<double>(n) / kSampleRate;
        const double s = std::sin(phase);
        const double c = std::cos(phase);
        sinDot += static_cast<long double>(output[n]) * s;
        cosDot += static_cast<long double>(output[n]) * c;
        sin2 += static_cast<long double>(s) * s;
        cos2 += static_cast<long double>(c) * c;
    }

    const double a = static_cast<double>(sinDot / sin2);
    const double b = static_cast<double>(cosDot / cos2);
    r.fundamental = std::sqrt(a * a + b * b) / std::sqrt(2.0);

    long double residue2 = 0.0;
    for (std::size_t n = kWarmup; n < kCount; ++n) {
        const double phase = 2.0 * kPi * frequency * static_cast<double>(n) / kSampleRate;
        const double fitted = a * std::sin(phase) + b * std::cos(phase);
        const double e = output[n] - fitted;
        residue2 += static_cast<long double>(e) * e;
    }
    r.aliasRms = std::sqrt(static_cast<double>(residue2 / static_cast<long double>(kCount - kWarmup)));
    r.aliasDbRelative = 20.0 * std::log10(std::max(r.aliasRms, 1.0e-30) / std::max(r.fundamental, 1.0e-30));
    return r;
}
} // namespace

int main() {
    std::cout << std::fixed << std::setprecision(9);
    std::array<Result, 3> results{};
    const std::array<int, 3> factors{{1, 2, 4}};
    bool pass = true;

    for (std::size_t i = 0; i < factors.size(); ++i) {
        results[i] = runAliasTest(factors[i]);
        const auto& r = results[i];
        std::cout << r.factor << "x: fundamental_rms=" << r.fundamental
                  << " alias_rms=" << r.aliasRms
                  << " alias=" << r.aliasDbRelative << " dBc"
                  << " finite=" << (r.finite ? "yes" : "NO") << '\n';
        pass = pass && r.finite;
    }

    const double reduction2x = results[0].aliasDbRelative - results[1].aliasDbRelative;
    const double reduction4x = results[0].aliasDbRelative - results[2].aliasDbRelative;
    const double advantage4x = results[1].aliasDbRelative - results[2].aliasDbRelative;
    std::cout << "2x alias reduction vs 1x: " << reduction2x << " dB\n";
    std::cout << "4x alias reduction vs 1x: " << reduction4x << " dB\n";
    std::cout << "4x alias advantage vs 2x: " << advantage4x << " dB\n";

    // Functional thresholds for this exact stress stimulus. 2x and 4x cannot
    // reasonably share one threshold here because the 2x internal Nyquist is
    // only 48 kHz; the strong tanh spectrum already self-aliases above it before
    // downsampling. Require 2x to provide a real improvement, while 4x must show
    // the much stronger rejection expected from its 192 kHz internal rate.
    if (reduction2x < 5.0 || results[1].aliasDbRelative > -20.0)
        pass = false;
    if (reduction4x < 25.0 || results[2].aliasDbRelative > -40.0)
        pass = false;
    if (advantage4x < 15.0)
        pass = false;

    if (!pass) {
        std::cerr << "Oversampling nonlinear alias diagnostic FAILED\n";
        return EXIT_FAILURE;
    }

    std::cout << "Oversampling nonlinear alias diagnostic PASSED\n";
    return EXIT_SUCCESS;
}
