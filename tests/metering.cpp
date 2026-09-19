#include "../source/metering.h"

#include <cmath>
#include <iostream>

int main() {
    MixEngine::Metering meter;
    meter.prepare(44100.0);
    meter.beginBlock();

    // Constant 0 dBFS should converge toward 1.0 in the RMS/VU detector.
    for (int i = 0; i < 44100; ++i)
        meter.push(1.0, 0.5);
    meter.publish();

    if (std::abs(meter.peakL() - 1.0) > 1.0e-12) return 1;
    if (std::abs(meter.peakR() - 0.5) > 1.0e-12) return 2;
    if (meter.vuL() < 0.99 || meter.vuL() > 1.001) return 3;
    if (meter.vuR() < 0.495 || meter.vuR() > 0.501) return 4;

    // At the selected reference level the needle must sit at the authentic
    // voltage-domain 0 VU position of a -20..+3 VU moving-coil scale.
    const double refDb = -14.0;
    const double refLinear = std::pow(10.0, refDb / 20.0);
    const double needle = MixEngine::vuNeedleNormalized(refLinear, refDb);
    const double minAmp = std::pow(10.0, -20.0 / 20.0);
    const double maxAmp = std::pow(10.0,   3.0 / 20.0);
    const double expectedZero = (1.0 - minAmp) / (maxAmp - minAmp);
    if (std::abs(needle - expectedZero) > 1.0e-9) return 5;
    if (MixEngine::vuScaleNormalizedFromDb(-20.0) != 0.0) return 6;
    if (std::abs(MixEngine::vuScaleNormalizedFromDb(3.0) - 1.0) > 1.0e-12) return 7;

    meter.reset();
    if (meter.peakL() != 0.0 || meter.peakR() != 0.0 ||
        meter.vuL() != 0.0 || meter.vuR() != 0.0) return 8;

    std::cout << "Metering PASS\n";
    return 0;
}
