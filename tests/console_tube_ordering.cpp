#include "nonlinear_cores.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

double dcBlock(double x, double coeff, double& x1, double& y1) {
    const double y = x - x1 + coeff * y1;
    x1 = x;
    y1 = y;
    return y;
}

}

int main() {
    constexpr double sampleRate = 48000.0;
    constexpr double pi = 3.14159265358979323846;
    const double dcCoeff = std::exp(-2.0 * pi * 8.0 / sampleRate);

    double legacyX1 = 0.0, legacyY1 = 0.0;
    double movedX1 = 0.0, movedY1 = 0.0;
    double sumSq = 0.0;
    double peak = 0.0;
    constexpr int count = 65536;

    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        const double x = 0.42 * std::sin(2.0 * pi * 997.0 * t)
                       + 0.17 * std::sin(2.0 * pi * 4013.0 * t)
                       + 0.08;
        const double low = 0.31 * x;
        const double high = x - low;

        const double console = MixEngine::processConsoleNonlinearCore(x, low, high, 2, 0.73);

        // Legacy production ordering: Console nonlinear -> Console DC block -> Tube.
        const double legacyDc = dcBlock(console, dcCoeff, legacyX1, legacyY1);
        const double legacy = MixEngine::processTubeNonlinearCore(legacyDc, 2, 0.67);

        // Tempting one-island ordering: Console nonlinear -> Tube -> Console DC block.
        // This is what we must NOT silently substitute unless it proves equivalent.
        const double combined = MixEngine::processTubeNonlinearCore(console, 2, 0.67);
        const double moved = dcBlock(combined, dcCoeff, movedX1, movedY1);

        const double e = legacy - moved;
        sumSq += e * e;
        peak = std::max(peak, std::abs(e));
    }

    const double rms = std::sqrt(sumSq / static_cast<double>(count));
    std::cout << "Legacy-vs-reordered Console/Tube DC RMS difference: " << rms << "\n";
    std::cout << "Legacy-vs-reordered Console/Tube DC peak difference: " << peak << "\n";

    if (!std::isfinite(rms) || !std::isfinite(peak)) {
        std::cerr << "FAILED: ordering diagnostic produced non-finite values\n";
        return 1;
    }

    // Diagnostic only: a non-zero result is expected and proves that moving the
    // Console DC blocker across Tube is a real DSP reorder, not an exact refactor.
    if (peak == 0.0) {
        std::cerr << "FAILED: ordering diagnostic unexpectedly found exact equivalence\n";
        return 1;
    }

    std::cout << "PASSED: live integration guard confirms legacy ordering must be preserved\n";
    return 0;
}
