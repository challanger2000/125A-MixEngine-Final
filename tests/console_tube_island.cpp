#include "nonlinear_cores.h"
#include "oversampling.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

// Independent copy of the currently shipped Tube equations. This remains here
// deliberately so the shared header cannot silently change Tube behaviour.
double legacyTubeCore(double x, int type, double amount) {
    const double a = std::clamp(amount, 0.0, 1.0);
    if (a <= 0.0) return x;
    double gain = 1.8, bias = 0.035, asym = 0.025;
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

}

int main() {
    MixEngine::OversamplingEngine os;
    double maxTubeCoreError = 0.0;
    double maxEcoError = 0.0;
    bool finite = true;

    for (int type = 0; type < 3; ++type) {
        for (int ai = 0; ai <= 20; ++ai) {
            const double amount = static_cast<double>(ai) / 20.0;
            for (int i = 0; i <= 20000; ++i) {
                const double x = -2.0 + 4.0 * static_cast<double>(i) / 20000.0;
                const double legacy = legacyTubeCore(x, type, amount);
                const double shared = MixEngine::processTubeNonlinearCore(x, type, amount);
                maxTubeCoreError = std::max(maxTubeCoreError, std::abs(legacy - shared));
            }
        }
    }

    for (int mode = 0; mode < 4; ++mode) {
        for (int type = 0; type < 3; ++type) {
            for (int i = 0; i <= 20000; ++i) {
                const double x = -2.0 + 4.0 * static_cast<double>(i) / 20000.0;
                const double low = 0.31 * x;
                const double high = x - low;
                const double drive = 0.73;
                const double tubeAmount = 0.67;

                const double reference = legacyTubeCore(
                    MixEngine::processConsoleNonlinearCore(x, low, high, mode, drive),
                    type, tubeAmount);
                const double eco = os.process(x, 1, [&](double v) {
                    return MixEngine::processTubeNonlinearCore(
                        MixEngine::processConsoleNonlinearCore(v, low, high, mode, drive),
                        type, tubeAmount);
                });

                maxEcoError = std::max(maxEcoError, std::abs(reference - eco));
                finite = finite && std::isfinite(eco);
            }
        }
    }

    for (int factor : {2, 4}) {
        os.reset();
        for (int i = 0; i <= 20000; ++i) {
            const double x = -2.0 + 4.0 * static_cast<double>(i) / 20000.0;
            const double low = 0.31 * x;
            const double high = x - low;
            const double y = os.process(x, factor, [&](double v) {
                return MixEngine::processTubeNonlinearCore(
                    MixEngine::processConsoleNonlinearCore(v, low, high, 2, 0.73),
                    2, 0.67);
            });
            finite = finite && std::isfinite(y);
        }
    }

    constexpr double kTolerance = 1.0e-15;
    std::cout << "Tube shared-core max error: " << maxTubeCoreError << "\n";
    std::cout << "Console+Tube shared island Eco max error: " << maxEcoError << "\n";
    std::cout << "Tolerance: " << kTolerance << "\n";
    std::cout << "Finite 1x/2x/4x: " << (finite ? "yes" : "no") << "\n";

    if (!finite || maxTubeCoreError > kTolerance || maxEcoError > kTolerance) {
        std::cerr << "FAILED: shared Console/Tube nonlinear cores are not legacy-equivalent/finite\n";
        return 1;
    }

    std::cout << "PASSED: shared Console/Tube nonlinear cores\n";
    return 0;
}
