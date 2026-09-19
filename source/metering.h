#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

namespace MixEngine {

// A moving-coil VU scale is proportional to signal voltage, not linearly spaced
// in dB. Map -20..+3 VU through amplitude so the telemetry and printed scale use
// exactly the same non-linear instrument geometry.
inline double vuScaleNormalizedFromDb(double vuDb) noexcept {
    constexpr double minDb = -20.0;
    constexpr double maxDb = 3.0;
    const double clamped = std::clamp(vuDb, minDb, maxDb);
    const double minAmp = std::pow(10.0, minDb / 20.0);
    const double maxAmp = std::pow(10.0, maxDb / 20.0);
    const double amp = std::pow(10.0, clamped / 20.0);
    return (amp - minAmp) / (maxAmp - minAmp);
}

// 0 VU is the selected reference level; the face spans -20 .. +3 VU.
inline double vuNeedleNormalized(double linear, double referenceDb) noexcept {
    const double dbfs = 20.0 * std::log10(std::max(linear, 1.0e-12));
    return vuScaleNormalizedFromDb(dbfs - referenceDb);
}

// Audio-thread detector with lock-free snapshots for the GUI/host telemetry.
// Detector state stays local to one audio stream; atomics are written once per block.
class Metering {
public:
    void prepare(double sampleRate) noexcept {
        const double sr = sampleRate > 1.0 ? sampleRate : 44100.0;
        // First-order VU-style envelope: about 99% settling after 300 ms.
        coeff_ = std::exp(std::log(0.01) / (0.300 * sr));
        prepared_ = true;
        reset();
    }

    void reset() noexcept {
        energyL_ = energyR_ = 0.0;
        blockPeakL_ = blockPeakR_ = 0.0;
        peakL_.store(0.0, std::memory_order_relaxed);
        peakR_.store(0.0, std::memory_order_relaxed);
        vuL_.store(0.0, std::memory_order_relaxed);
        vuR_.store(0.0, std::memory_order_relaxed);
    }

    void beginBlock() noexcept { blockPeakL_ = blockPeakR_ = 0.0; }

    void push(double left, double right) noexcept {
        blockPeakL_ = std::max(blockPeakL_, std::abs(left));
        blockPeakR_ = std::max(blockPeakR_, std::abs(right));
        const double oneMinus = 1.0 - coeff_;
        energyL_ = coeff_ * energyL_ + oneMinus * left * left;
        energyR_ = coeff_ * energyR_ + oneMinus * right * right;
    }

    // Compatibility helper for the standalone objective test.
    void push(double left, double right, double sampleRate) noexcept {
        if (!prepared_) {
            const double sr = sampleRate > 1.0 ? sampleRate : 44100.0;
            coeff_ = std::exp(std::log(0.01) / (0.300 * sr));
            prepared_ = true;
        }
        push(left, right);
        publish();
    }

    void publish() noexcept {
        peakL_.store(blockPeakL_, std::memory_order_relaxed);
        peakR_.store(blockPeakR_, std::memory_order_relaxed);
        vuL_.store(std::sqrt(std::max(0.0, energyL_)), std::memory_order_relaxed);
        vuR_.store(std::sqrt(std::max(0.0, energyR_)), std::memory_order_relaxed);
    }

    double peakL() const noexcept { return peakL_.load(std::memory_order_relaxed); }
    double peakR() const noexcept { return peakR_.load(std::memory_order_relaxed); }
    double vuL() const noexcept { return vuL_.load(std::memory_order_relaxed); }
    double vuR() const noexcept { return vuR_.load(std::memory_order_relaxed); }

private:
    double coeff_ = 0.9999;
    double energyL_ = 0.0, energyR_ = 0.0;
    double blockPeakL_ = 0.0, blockPeakR_ = 0.0;
    bool prepared_ = false;
    std::atomic<double> peakL_{0.0}, peakR_{0.0};
    std::atomic<double> vuL_{0.0}, vuR_{0.0};
};

} // namespace MixEngine
