#include "analog_models_v2.h"
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

    // V2 Tube is stateful. Amount=0 must retain a subtle enabled-stage
    // fingerprint, all three voices must stay finite/bounded, and Eco (1x)
    // must be exactly equivalent to direct model evaluation.
    for (int type = 0; type < 3; ++type) {
        for (int ai = 0; ai <= 20; ++ai) {
            const double amount = static_cast<double>(ai) / 20.0;
            MixEngine::TubeModelState state;
            for (int i = 0; i <= 24000; ++i) {
                const double x = -3.0 + 6.0 * static_cast<double>(i) / 24000.0;
                const double y = MixEngine::processTubeModelV2(x, state, type, amount, 48000.0);
                finite = finite && std::isfinite(y);
                maxAbs = std::max(maxAbs, std::abs(y));
                if (amount == 0.0 && std::abs(x) <= 1.0 && i > 4000) {
                    const double d = y - x;
                    maxBaseDelta = std::max(maxBaseDelta, std::abs(d));
                    baseDeltaSq += d * d;
                    ++baseDeltaCount;
                }
            }
        }
    }

    // Factor 1 of the oversampling wrapper must be bit-transparent around the
    // V2 Console -> Tube evaluation sequence, including Tube memory.
    for (int mode = 0; mode < 4; ++mode) {
        for (int type = 0; type < 3; ++type) {
            os.reset();
            MixEngine::TubeModelState directState;
            MixEngine::TubeModelState ecoState;
            for (int i = 0; i <= 20000; ++i) {
                const double x = -2.0 + 4.0 * static_cast<double>(i) / 20000.0;
                const double low = 0.31 * x;
                const double high = x - low;
                const double drive = 0.73;
                const double tubeAmount = 0.67;

                const double console =
                    MixEngine::processConsoleNonlinearCore(x, low, high, mode, drive);
                const double reference =
                    MixEngine::processTubeModelV2(console, directState, type, tubeAmount, 48000.0);
                const double eco = os.process(x, 1, [&](double v) {
                    const double cv =
                        MixEngine::processConsoleNonlinearCore(v, low, high, mode, drive);
                    return MixEngine::processTubeModelV2(cv, ecoState, type, tubeAmount, 48000.0);
                });

                maxEcoError = std::max(maxEcoError, std::abs(reference - eco));
                finite = finite && std::isfinite(eco);
            }
        }
    }

    // Higher quality factors must remain finite under deliberately hot drive.
    for (int factor : {2, 4}) {
        os.reset();
        MixEngine::TubeModelState state;
        for (int i = 0; i <= 20000; ++i) {
            const double x = -2.0 + 4.0 * static_cast<double>(i) / 20000.0;
            const double low = 0.31 * x;
            const double high = x - low;
            const double y = os.process(x, factor, [&](double v) {
                const double cv =
                    MixEngine::processConsoleNonlinearCore(v, low, high, 2, 0.73);
                return MixEngine::processTubeModelV2(
                    cv, state, 2, 0.67, 48000.0 * static_cast<double>(factor));
            });
            finite = finite && std::isfinite(y);
            maxAbs = std::max(maxAbs, std::abs(y));
        }
    }

    constexpr double kExactTolerance = 1.0e-15;
    constexpr double kSanityBound = 6.0;
    const double baseDeltaRms = baseDeltaCount > 0
        ? std::sqrt(baseDeltaSq / static_cast<double>(baseDeltaCount)) : 0.0;

    std::cout << "V2 Tube Amount=0 base-colour max delta (+/-1): " << maxBaseDelta << "\n";
    std::cout << "V2 Tube Amount=0 base-colour RMS delta (+/-1): " << baseDeltaRms << "\n";
    std::cout << "V2 Console+Tube Eco max error: " << maxEcoError << "\n";
    std::cout << "V2 Tube/island maximum absolute output: " << maxAbs << "\n";
    std::cout << "Finite 1x/2x/4x: " << (finite ? "yes" : "no") << "\n";

    // Keep Amount=0 perceptible but restrained. Exact sonic limits will be
    // tightened after the first characterization run.
    if (!finite || maxBaseDelta < 1.0e-6 || maxBaseDelta > 0.05 ||
        baseDeltaRms < 1.0e-7 || baseDeltaRms > 0.025 ||
        maxEcoError > kExactTolerance || maxAbs > kSanityBound) {
        std::cerr << "FAILED: V2 Console/Tube nonlinear contract\n";
        return 1;
    }

    std::cout << "PASSED: V2 Console/Tube nonlinear contract\n";
    return 0;
}
