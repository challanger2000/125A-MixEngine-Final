#include "oversampling.h"
#include "media_oversampling_live.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;
constexpr int kCount = 65536;
constexpr int kWarmup = 4096;

using MixEngine::OversamplingEngine;

double tapeLiveCore(double x, double shape) {
    const double k = std::max(0.0, shape - 1.0);
    const double vv = x * x;
    return x / std::sqrt(1.0 + (1.35 * k) * vv);
}
double vinylLegacy(double x, double drive) { return std::tanh(x * drive) / drive; }

template <typename Fn>
bool ecoExact(Fn&& fn, const std::vector<double>& params) {
    for (double p : params) {
        OversamplingEngine e;
        for (int i = 0; i < 20000; ++i) {
            const double x = 1.7 * std::sin(0.013 * i) + 0.31 * std::sin(0.071 * i);
            const double legacy = fn(x, p);
            const double os = e.process(x, 1, [&](double v) { return fn(v, p); });
            if (legacy != os) return false;
        }
    }
    return true;
}

template <typename Fn>
double residualRms(Fn&& fn, double param, int factor, double frequency) {
    OversamplingEngine e;
    std::vector<double> y(kCount);
    for (int n = 0; n < kCount; ++n) {
        const double x = 0.95 * std::sin(2.0 * kPi * frequency * n / kSampleRate);
        y[n] = e.process(x, factor, [&](double v) { return fn(v, param); });
        if (!std::isfinite(y[n])) return 1e9;
    }
    double ss = 0.0, cc = 0.0, sc = 0.0, sy = 0.0, cy = 0.0;
    for (int n = kWarmup; n < kCount; ++n) {
        const double s = std::sin(2.0 * kPi * frequency * n / kSampleRate);
        const double c = std::cos(2.0 * kPi * frequency * n / kSampleRate);
        ss += s*s; cc += c*c; sc += s*c; sy += s*y[n]; cy += c*y[n];
    }
    const double det = ss*cc-sc*sc;
    const double a = (sy*cc-cy*sc)/det, b = (cy*ss-sy*sc)/det;
    double err = 0.0; int count = 0;
    for (int n = kWarmup; n < kCount; ++n) {
        const double fit = a*std::sin(2.0*kPi*frequency*n/kSampleRate)+b*std::cos(2.0*kPi*frequency*n/kSampleRate);
        const double d = y[n]-fit; err += d*d; ++count;
    }
    return std::sqrt(err/count);
}

template <typename Fn>
bool verify(const char* name, Fn&& fn, const std::vector<double>& params) {
    bool ok = ecoExact(fn, params);
    std::cout << name << " Eco exact: " << (ok ? "yes" : "NO") << '\n';
    for (double p : params) for (double f : {9000.0, 15000.0}) {
        const double r1=residualRms(fn,p,1,f), r2=residualRms(fn,p,2,f), r4=residualRms(fn,p,4,f);
        const bool finite=std::isfinite(r1)&&std::isfinite(r2)&&std::isfinite(r4)&&r1<1e8&&r2<1e8&&r4<1e8;
        const bool monotonic=finite && r2 < r1 && r4 <= r2 * 1.001;
        std::cout << name << " param=" << p << " f=" << f << "Hz residual 1x=" << r1 << " 2x=" << r2 << " 4x=" << r4
                  << " improvement2=" << 20.0*std::log10(r1/r2) << "dB improvement4=" << 20.0*std::log10(r1/r4) << "dB "
                  << (monotonic ? "PASS" : "FAIL") << '\n';
        ok = ok && monotonic;
    }
    return ok;
}
}

int main() {
    // Exact live ranges: Tape shape = 1 + 0.42*amount -> [1,1.42].
    // Vinyl drive = 1 + 0.35*character + 0.25*wear -> [1,1.60].
    const bool tape = verify("Tape", tapeLiveCore, {1.0, 1.21, 1.42});
    const bool vinyl = verify("Vinyl", vinylLegacy, {1.0, 1.30, 1.60});
    if (!(tape && vinyl)) {
        std::cerr << "FAILED: Tape/Vinyl oversampling verification\n";
        return 1;
    }
    std::cout << "PASSED: Tape/Vinyl Eco equivalence, finite processing and alias reduction\n";
    return 0;
}
