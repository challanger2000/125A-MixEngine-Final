#include "stereo_field.h"

#include <cmath>
#include <iostream>

namespace {
double toDb(double gain) {
    return 20.0 * std::log10(std::max(gain, 1.0e-12));
}

double measureHighSide(double depthBipolar) {
    constexpr double sampleRate = 48000.0;
    constexpr double frequency = 8000.0;
    constexpr int total = 48000;
    constexpr int skip = 8000;
    constexpr double pi = 3.14159265358979323846;

    MixEngine::StereoFieldState state;
    const double lowMonoCoeff = MixEngine::stereoOnePoleCoefficient(120.0, sampleRate);
    const double depthCoeff = MixEngine::stereoOnePoleCoefficient(2000.0, sampleRate);
    const double depthGain = MixEngine::stereoDepthGain(depthBipolar);

    double energy = 0.0;
    int count = 0;
    for (int n = 0; n < total; ++n) {
        const double x = std::sin(2.0 * pi * frequency * static_cast<double>(n) / sampleRate);
        double l = x;
        double r = -x; // pure Side signal
        MixEngine::processStereoFieldSample(l, r, state, 1.0, 0.0,
                                            lowMonoCoeff, depthGain, depthCoeff);
        if (n >= skip) {
            const double side = 0.5 * (l - r);
            energy += side * side;
            ++count;
        }
    }
    return std::sqrt(energy / static_cast<double>(count));
}
}

int main() {
    const double neutral = MixEngine::stereoDepthGain(0.0);
    const double back = MixEngine::stereoDepthGain(1.0);
    const double forward = MixEngine::stereoDepthGain(-1.0);

    if (std::abs(neutral - 1.0) > 1.0e-15) return 1;
    if (std::abs(toDb(back) + 4.0) > 1.0e-12) return 2;
    if (std::abs(toDb(forward) - 4.0) > 1.0e-12) return 3;

    // WIDTH 0 must collapse a stereo pair to its Mid component.
    {
        MixEngine::StereoFieldState state;
        double l = 0.7, r = -0.3;
        MixEngine::processStereoFieldSample(l, r, state, 0.0, 0.0, 0.01, 1.0, 0.1);
        if (std::abs(l - 0.2) > 1.0e-12 || std::abs(r - 0.2) > 1.0e-12) return 4;
    }

    // WIDTH 100%, LOW MONO 0%, DEPTH 0 must be transparent.
    {
        MixEngine::StereoFieldState state;
        double l = 0.37, r = -0.21;
        const double inL = l, inR = r;
        MixEngine::processStereoFieldSample(l, r, state, 1.0, 0.0, 0.01, 1.0, 0.1);
        if (std::abs(l - inL) > 1.0e-12 || std::abs(r - inR) > 1.0e-12) return 5;
    }

    // At a clearly high Side frequency, positive DEPTH must move it back and
    // negative DEPTH must bring it forward by an unmistakable amount.
    const double rmsNeutral = measureHighSide(0.0);
    const double rmsBack = measureHighSide(1.0);
    const double rmsForward = measureHighSide(-1.0);
    const double backDb = toDb(rmsBack / rmsNeutral);
    const double forwardDb = toDb(rmsForward / rmsNeutral);
    if (!(backDb < -2.5)) return 6;
    if (!(forwardDb > 2.5)) return 7;

    // LOW MONO at 100% must strongly suppress a steady low-frequency Side signal.
    {
        constexpr double sr = 48000.0;
        MixEngine::StereoFieldState state;
        const double lowCoeff = MixEngine::stereoOnePoleCoefficient(120.0, sr);
        const double depthCoeff = MixEngine::stereoOnePoleCoefficient(2000.0, sr);
        double side = 1.0;
        for (int n = 0; n < 30000; ++n) {
            double l = 1.0, r = -1.0;
            MixEngine::processStereoFieldSample(l, r, state, 1.0, 1.0,
                                                lowCoeff, 1.0, depthCoeff);
            side = 0.5 * (l - r);
        }
        if (std::abs(side) > 1.0e-6) return 8;
    }

    std::cout << "Stereo field PASS: DEPTH back=" << backDb
              << " dB, forward=" << forwardDb << " dB\n";
    return 0;
}
