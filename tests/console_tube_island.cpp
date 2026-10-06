#include "nonlinear_cores.h"
#include "oversampling.h"

#include <algorithm>
#include <cmath>
#include <iostream>

int main() {
    MixEngine::OversamplingEngine os;
    double maxEcoError = 0.0;
    bool finite = true;

    // 1x must be an exact pass-through wrapper around the *current* shared
    // Console+Tube core. This guards island wiring without freezing old DSP.
    for (int mode = 0; mode < 4; ++mode) {
        for (double typeMorph : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            for (double tubeAmount : {0.0, 0.2, 0.5, 0.8, 1.0}) {
                os.reset();
                for (int i = 0; i <= 20000; ++i) {
                    const double x = -2.0 + 4.0 * static_cast<double>(i) / 20000.0;
                    const double low = 0.31 * x;
                    const double high = x - low;
                    const double drive = 0.73;

                    const double reference = MixEngine::processTubeNonlinearCore(
                        MixEngine::processConsoleNonlinearCore(x, low, high, mode, drive),
                        typeMorph, tubeAmount);
                    const double eco = os.process(x, 1, [&](double v) {
                        return MixEngine::processTubeNonlinearCore(
                            MixEngine::processConsoleNonlinearCore(v, low, high, mode, drive),
                            typeMorph, tubeAmount);
                    });

                    maxEcoError = std::max(maxEcoError, std::abs(reference - eco));
                    finite = finite && std::isfinite(reference) && std::isfinite(eco);
                }
            }
        }
    }

    // 2x/4x need not equal 1x; their job is anti-aliasing. They must remain
    // finite and bounded across the full current morph/drive range.
    double peak = 0.0;
    for (int factor : {2, 4}) {
        for (int mode = 0; mode < 4; ++mode) {
            for (double typeMorph : {0.0, 0.25, 0.5, 0.75, 1.0}) {
                os.reset();
                for (int i = 0; i <= 20000; ++i) {
                    const double x = -2.0 + 4.0 * static_cast<double>(i) / 20000.0;
                    const double low = 0.31 * x;
                    const double high = x - low;
                    const double y = os.process(x, factor, [&](double v) {
                        return MixEngine::processTubeNonlinearCore(
                            MixEngine::processConsoleNonlinearCore(v, low, high, mode, 1.0),
                            typeMorph, 1.0);
                    });
                    finite = finite && std::isfinite(y);
                    peak = std::max(peak, std::abs(y));
                }
            }
        }
    }

    constexpr double kTolerance = 1.0e-14;
    std::cout << "Console+Tube current-core Eco max error: " << maxEcoError << "\n";
    std::cout << "2x/4x peak: " << peak << "\n";
    std::cout << "Finite 1x/2x/4x: " << (finite ? "yes" : "no") << "\n";

    if (!finite || maxEcoError > kTolerance || peak > 8.0) {
        std::cerr << "FAILED: current Console/Tube island wiring or boundedness\n";
        return 1;
    }

    std::cout << "PASSED: current Console/Tube island wiring, morphs and oversampling boundedness\n";
    return 0;
}
