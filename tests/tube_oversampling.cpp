#include "oversampling.h"
#include "analog_models_v2.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kFs = 48000.0;
constexpr std::size_t kWarmup = 8192;
constexpr std::size_t kCount = 131072;

double residualDbc(int factor, double frequency, int type) {
    MixEngine::OversamplingEngine os;
    MixEngine::TubeModelState state;
    long double ss = 0.0, cc = 0.0, ys = 0.0, yc = 0.0;

    // 131072 doubles are about 1 MiB. Keeping this as std::array on the stack
    // exhausted the default Windows executable stack before the diagnostic
    // could write its report. Allocate the analysis buffer on the heap instead.
    std::vector<double> y(kCount);

    for (std::size_t n = 0; n < kCount; ++n) {
        const double x = 0.72 * std::sin(2.0 * kPi * frequency * static_cast<double>(n) / kFs);
        y[n] = os.process(x, factor, [&](double v) { return MixEngine::processTubeModelV2(v, state, type, 1.0, kFs * static_cast<double>(factor)); });
        if (!std::isfinite(y[n]))
            return 999.0;
    }

    for (std::size_t n = kWarmup; n < kCount; ++n) {
        const double p = 2.0 * kPi * frequency * static_cast<double>(n) / kFs;
        const double s = std::sin(p), c = std::cos(p);
        ss += static_cast<long double>(s) * s;
        cc += static_cast<long double>(c) * c;
        ys += static_cast<long double>(y[n]) * s;
        yc += static_cast<long double>(y[n]) * c;
    }
    const double a = static_cast<double>(ys / ss);
    const double b = static_cast<double>(yc / cc);
    const double fundamentalRms = std::sqrt(a * a + b * b) / std::sqrt(2.0);

    long double e2 = 0.0;
    for (std::size_t n = kWarmup; n < kCount; ++n) {
        const double p = 2.0 * kPi * frequency * static_cast<double>(n) / kFs;
        const double fit = a * std::sin(p) + b * std::cos(p);
        const double e = y[n] - fit;
        e2 += static_cast<long double>(e) * e;
    }
    const double residualRms = std::sqrt(static_cast<double>(e2 / (kCount - kWarmup)));
    return 20.0 * std::log10(std::max(residualRms, 1.0e-30) / std::max(fundamentalRms, 1.0e-30));
}
} // namespace

int main() {
    bool finite = true;
    bool monotonic = true;
    std::ostringstream report;
    report << std::fixed << std::setprecision(3);
    report << "125A MixEngine - real Tube oversampling diagnostic\n";

    for (int type = 0; type < 3; ++type) {
        for (double f : {9000.0, 15000.0}) {
            const double d1 = residualDbc(1, f, type);
            const double d2 = residualDbc(2, f, type);
            const double d4 = residualDbc(4, f, type);
            const bool rowFinite = std::isfinite(d1) && std::isfinite(d2) && std::isfinite(d4);
            // 2x must materially improve over 1x. At 9 kHz, 2x and 4x can
            // converge to the same residual/filter floor; allow only a tiny
            // 0.02 dB numerical tolerance there rather than requiring a
            // meaningless strict floating-point inequality.
            const bool rowMonotonic =
                rowFinite && (d2 < d1 - 0.10) && (d4 <= d2 + 0.02);

            report << "Tube " << type << " @ " << f << " Hz: 1x=" << d1
                   << " dBc 2x=" << d2 << " dBc 4x=" << d4
                   << " dBc | 2x-vs-1x=" << (d1 - d2)
                   << " dB 4x-vs-2x=" << (d2 - d4)
                   << " dB | " << (rowMonotonic ? "monotonic" : "NOT monotonic") << '\n';

            finite = finite && rowFinite;
            monotonic = monotonic && rowMonotonic;
        }
    }

    report << "Summary: finite=" << (finite ? "yes" : "NO")
           << " monotonic=" << (monotonic ? "yes" : "NO") << '\n';
    report << (finite && monotonic
                   ? "Real Tube oversampling verification PASSED\n"
                   : "Real Tube oversampling verification FAILED\n");

    const std::string text = report.str();
    std::ofstream file("tube_oversampling_report.txt", std::ios::out | std::ios::trunc);
    if (!file) {
        std::cerr << "Could not create tube_oversampling_report.txt\n";
        return 2;
    }
    file << text;
    file.flush();
    file.close();

    std::cout << text << std::flush;
    return (finite && monotonic) ? EXIT_SUCCESS : EXIT_FAILURE;
}
