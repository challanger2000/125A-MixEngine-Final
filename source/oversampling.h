#pragma once

#include "hiir/Upsampler2xF64Fpu.h"
#include "hiir/Downsampler2xF64Fpu.h"
#include "hiir/PolyphaseIir2Designer.h"

#include <algorithm>
#include <array>
#include <utility>

namespace MixEngine {

// Shared 1x/2x/4x oversampling engine for one audio lane.
// The caller supplies the nonlinear processing callback.  This class owns only
// the sample-rate conversion state so Channel and MixEngine can use the same
// implementation without duplicating Console/Tube/Tape/Vinyl oversamplers.
class OversamplingEngine {
public:
    OversamplingEngine() { initialiseCoefficients(); reset(); }

    void reset() {
        upStage1_.clear_buffers();
        downStage1_.clear_buffers();
        upStage2_.clear_buffers();
        downStage2_.clear_buffers();
        configureCoefficients();
    }

    template <typename NonlinearFn>
    double process(double input, int factor, NonlinearFn&& nonlinear) {
        factor = sanitiseFactor(factor);
        if (factor == 1)
            return nonlinear(input);

        double stage1[2]{};
        upStage1_.process_sample(stage1[0], stage1[1], input);

        if (factor == 2) {
            stage1[0] = nonlinear(stage1[0]);
            stage1[1] = nonlinear(stage1[1]);
            return downStage1_.process_sample(stage1);
        }

        // Preserve chronological order: each 2x Stage-1 sample expands into
        // two adjacent Stage-2 samples.  They are collapsed in the same order.
        double stage2a[2]{};
        double stage2b[2]{};
        upStage2_.process_sample(stage2a[0], stage2a[1], stage1[0]);
        upStage2_.process_sample(stage2b[0], stage2b[1], stage1[1]);

        stage2a[0] = nonlinear(stage2a[0]);
        stage2a[1] = nonlinear(stage2a[1]);
        stage2b[0] = nonlinear(stage2b[0]);
        stage2b[1] = nonlinear(stage2b[1]);

        double collapsed[2] = {
            downStage2_.process_sample(stage2a),
            downStage2_.process_sample(stage2b)
        };
        return downStage1_.process_sample(collapsed);
    }

    static int sanitiseFactor(int factor) noexcept {
        return factor >= 4 ? 4 : (factor >= 2 ? 2 : 1);
    }

private:
    void initialiseCoefficients() {
        hiir::PolyphaseIir2Designer::compute_coefs_spec_order_tbw(stage1Coefficients_.data(), 13, 0.01);
        hiir::PolyphaseIir2Designer::compute_coefs_spec_order_tbw(stage2Coefficients_.data(), 4, 0.255);
    }

    void configureCoefficients() {
        upStage1_.set_coefs(stage1Coefficients_.data());
        downStage1_.set_coefs(stage1Coefficients_.data());
        upStage2_.set_coefs(stage2Coefficients_.data());
        downStage2_.set_coefs(stage2Coefficients_.data());
    }

    std::array<double, 13> stage1Coefficients_{};
    std::array<double, 4> stage2Coefficients_{};
    hiir::Upsampler2xF64Fpu<13> upStage1_{};
    hiir::Downsampler2xF64Fpu<13> downStage1_{};
    hiir::Upsampler2xF64Fpu<4> upStage2_{};
    hiir::Downsampler2xF64Fpu<4> downStage2_{};
};

} // namespace MixEngine
