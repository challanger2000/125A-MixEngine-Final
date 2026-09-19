#include "nonlinear_cores.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

std::uint32_t next(std::uint32_t& s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s;
}

double bipolar(std::uint32_t& s) {
    return (static_cast<double>(next(s)) / 2147483647.5) - 1.0;
}

} // namespace

int main() {
    std::uint32_t rng = 0x125A2026u;
    double maxZeroDriveError = 0.0;
    double maxNonFiniteGuard = 0.0;
    double maxDrivenDifference = 0.0;

    for (int mode = 0; mode < 4; ++mode) {
        for (int i = 0; i < 50000; ++i) {
            const double x = 2.0 * bipolar(rng);
            const double low = 2.0 * bipolar(rng);
            const double high = 2.0 * bipolar(rng);

            const double zero = MixEngine::processConsoleNonlinearCore(x, low, high, mode, 0.0);
            maxZeroDriveError = std::max(maxZeroDriveError, std::abs(zero - x));

            const double driven = MixEngine::processConsoleNonlinearCore(x, low, high, mode, 1.0);
            if (!std::isfinite(driven))
                maxNonFiniteGuard = 1.0;
            maxDrivenDifference = std::max(maxDrivenDifference, std::abs(driven - x));
        }
    }

    std::cout << "Console DRIVE=0 max identity error: " << maxZeroDriveError << '\n';
    std::cout << "Console DRIVE=100 max driven difference: " << maxDrivenDifference << '\n';

    if (maxZeroDriveError != 0.0) {
        std::cerr << "FAILED: Console DRIVE=0 is not an exact nonlinear-core identity\n";
        return 1;
    }
    if (maxNonFiniteGuard != 0.0) {
        std::cerr << "FAILED: Console driven core produced non-finite output\n";
        return 1;
    }
    if (maxDrivenDifference < 1.0e-4) {
        std::cerr << "FAILED: Console DRIVE=100 does not produce meaningful shaping\n";
        return 1;
    }

    std::cout << "PASSED: Console Drive contract is neutral at zero and active when driven\n";
    return 0;
}
