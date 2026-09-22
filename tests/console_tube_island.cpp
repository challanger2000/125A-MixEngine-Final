#include "nonlinear_cores.h"
#include "oversampling.h"

#include <algorithm>
#include <cmath>
#include <iostream>

int main() {
    MixEngine::OversamplingEngine os;
    double maxBaseDelta = 0.0;
    double baseDeltaSq = 0.0;
    std::size_t baseDeltaCount = 0;
    double maxEcoError = 0.0;
    double maxAbs = 0.0;
    bool finite = true;

    // With Tube enabled, Amount=0 intentionally retains a subtle hardware-like
    // base character. It must be non-zero but restrained over the normal +/-1
    // signal range, while all settings remain finite and bounded when driven hot.
    for (int type = 0; type < 3; ++type) {
        for (int ai = 0; ai <= 20; ++ai) {
            const double amount = static_cast<double>(ai) / 20.0;
            for (int i = 0; i <= 24000; ++i) {
                const double x = -3.0 + 6.0 * static_cast<double>(i) / 24000.0;
                const double y = MixEngine::processTubeNonlinearCore(x, type, amount);
                finite = finite && std::isfinite(y);
                maxAbs = std::max(maxAbs, std::abs(y));
                if (amount == 0.0 && std::abs(x) <= 1.0) {
                    const double d = y - x;
                    maxBaseDelta = std::max(maxBaseDelta, std::abs(d));
                    baseDeltaSq += d * d;
                    ++baseDeltaCount;
                }
            }
        }
    }

    // Factor 1 of the oversampling wrapper is required to be bit-transparent
    // around the current live Console -> Tube nonlinear island.
    for (int mode = 0; mode < 4; ++mode) {
        for (int type = 0; type < 3; ++type) {
            os.reset();
            for (int i = 0; i <= 20000; ++i) {
                const double x = -2.0 + 4.0 * static_cast<double>(i) / 20000.0;
                const double low = 0.31 * x;
                const double high = x - low;
                const double drive = 0.73;
                const double tubeAmount = 0.67;

                const double reference = MixEngine::processTubeNonlinearCore(
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

    // Higher quality factors must remain finite under the same live island.
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
            maxAbs = std::max(maxAbs, std::abs(y));
        }
    }

    constexpr double kExactTolerance = 1.0e-15;
    constexpr double kSanityBound = 8.0;
    const double baseDeltaRms = baseDeltaCount > 0
        ? std::sqrt(baseDeltaSq / static_cast<double>(baseDeltaCount)) : 0.0;
    std::cout << "Tube Amount=0 base-colour max delta (+/-1): " << maxBaseDelta << "\n";
    std::cout << "Tube Amount=0 base-colour RMS delta (+/-1): " << baseDeltaRms << "\n";
    std::cout << "Console+Tube live island Eco max error: " << maxEcoError << "\n";
    std::cout << "Tube/island maximum absolute output: " << maxAbs << "\n";
    std::cout << "Finite 1x/2x/4x: " << (finite ? "yes" : "no") << "\n";

    if (!finite || maxBaseDelta < 1.0e-5 || maxBaseDelta > 0.02 ||
        baseDeltaRms < 1.0e-6 || baseDeltaRms > 0.01 ||
        maxEcoError > kExactTolerance || maxAbs > kSanityBound) {
        std::cerr << "FAILED: live Console/Tube nonlinear island contract\n";
        return 1;
    }

    std::cout << "PASSED: live Console/Tube nonlinear island contract\n";
    return 0;
}
