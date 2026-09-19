#include "oversampling.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

namespace {
constexpr double kPi = 3.1415926535897932384626433832795;

struct Result {
    int factor{};
    int delay{};
    double gainDb{};
    double rmsError{};
    double peakError{};
    bool finite{true};
};

double rms(const std::vector<double>& x, std::size_t begin, std::size_t end) {
    long double sum = 0.0;
    for (std::size_t i = begin; i < end; ++i)
        sum += static_cast<long double>(x[i]) * x[i];
    return std::sqrt(static_cast<double>(sum / static_cast<long double>(end - begin)));
}

Result runRoundtrip(int factor) {
    constexpr double sampleRate = 48000.0;
    constexpr std::size_t count = 65536;
    constexpr std::size_t warmup = 4096;
    constexpr int maxDelay = 128;

    std::vector<double> input(count);
    std::vector<double> output(count);
    for (std::size_t n = 0; n < count; ++n) {
        const double t = static_cast<double>(n) / sampleRate;
        input[n] = 0.31 * std::sin(2.0 * kPi * 997.0 * t)
                 + 0.17 * std::sin(2.0 * kPi * 4013.0 * t + 0.3)
                 + 0.09 * std::sin(2.0 * kPi * 9137.0 * t + 0.7);
    }

    MixEngine::OversamplingEngine os;
    for (std::size_t n = 0; n < count; ++n)
        output[n] = os.process(input[n], factor, [](double x) { return x; });

    Result r{};
    r.factor = factor;
    for (double x : output)
        r.finite = r.finite && std::isfinite(x);

    // HIIR IIR half-band filters are not linear phase.  Find the integer delay
    // that maximises correlation, then evaluate passband gain and residual.
    double bestCorr = -std::numeric_limits<double>::infinity();
    for (int d = 0; d <= maxDelay; ++d) {
        long double corr = 0.0;
        for (std::size_t n = warmup + d; n < count; ++n)
            corr += static_cast<long double>(input[n - d]) * output[n];
        if (static_cast<double>(corr) > bestCorr) {
            bestCorr = static_cast<double>(corr);
            r.delay = d;
        }
    }

    const std::size_t begin = warmup + static_cast<std::size_t>(r.delay);
    const std::size_t end = count;
    const double inRms = rms(input, warmup, end - static_cast<std::size_t>(r.delay));
    const double outRms = rms(output, begin, end);
    r.gainDb = 20.0 * std::log10(outRms / inRms);

    long double err2 = 0.0;
    double peak = 0.0;
    for (std::size_t n = begin; n < end; ++n) {
        const double e = output[n] - input[n - static_cast<std::size_t>(r.delay)];
        err2 += static_cast<long double>(e) * e;
        peak = std::max(peak, std::abs(e));
    }
    r.rmsError = std::sqrt(static_cast<double>(err2 / static_cast<long double>(end - begin)));
    r.peakError = peak;
    return r;
}
} // namespace

int main() {
    std::cout << std::fixed << std::setprecision(9);
    bool pass = true;
    for (int factor : {1, 2, 4}) {
        const Result r = runRoundtrip(factor);
        std::cout << factor << "x: delay=" << r.delay
                  << " gain=" << r.gainDb << " dB"
                  << " rms_error=" << r.rmsError
                  << " peak_error=" << r.peakError
                  << " finite=" << (r.finite ? "yes" : "NO") << '\n';

        // Identity path must preserve level. Residual is reported rather than
        // treated as a hard linear-phase requirement because HIIR is IIR.
        if (!r.finite || std::abs(r.gainDb) > 0.05)
            pass = false;
        if (factor == 1 && (r.delay != 0 || r.rmsError > 1.0e-12 || r.peakError > 1.0e-12))
            pass = false;
    }

    if (!pass) {
        std::cerr << "Oversampling roundtrip diagnostic FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "Oversampling roundtrip diagnostic PASSED\n";
    return EXIT_SUCCESS;
}
