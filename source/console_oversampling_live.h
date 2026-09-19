#pragma once

#include "nonlinear_cores.h"

namespace MixEngine {

template <typename Engine>
inline double processConsoleOversampledCore(Engine& engine,
                                            int& currentFactor,
                                            int factor,
                                            double x,
                                            double low,
                                            double high,
                                            int mode,
                                            double drive) {
    if (currentFactor != factor) {
        engine.reset();
        currentFactor = factor;
    }
    return engine.process(x, factor, [&](double osX) {
        return processConsoleNonlinearCore(osX, low, high, mode, drive);
    });
}

} // namespace MixEngine
