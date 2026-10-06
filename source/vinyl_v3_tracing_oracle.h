#pragma once
#include <algorithm>
#include <cmath>

namespace MixEngine::V3Research {

constexpr double kVinylPi=3.14159265358979323846;

struct VinylTracingParameters {
    double rpm=33.3333333333333;
    double grooveRadiusM=0.100;
    double stylusRadiusM=5.0e-6;
    double velocityPeakMps=0.12;
};

inline double vinylGrooveSpeed(const VinylTracingParameters& p) noexcept {
    return p.grooveRadiusM*(2.0*kVinylPi*p.rpm/60.0);
}

inline double vinylSineAmplitude(double frequency,const VinylTracingParameters& p) noexcept {
    return p.velocityPeakMps/(2.0*kVinylPi*std::max(1.0,frequency));
}

// Offline/reference tracing model derived independently from the spherical-tip
// geometry summarized by Jovanovic (JAES 71(10), 2023). Not realtime DSP.
class VinylTracingOracle {
public:
    explicit VinylTracingOracle(VinylTracingParameters p={}) noexcept : p_(p) {}
    void reset() noexcept { previousEpsilon_=0.0; }

    double processSinePhase(double phase,double frequency) noexcept {
        const double rho=std::max(1.0e-9,p_.stylusRadiusM);
        const double speed=std::max(1.0e-6,vinylGrooveSpeed(p_));
        const double A=vinylSineAmplitude(frequency,p_);
        const double k=2.0*kVinylPi*frequency/speed;

        double eps=std::clamp(previousEpsilon_,-0.95*rho,0.95*rho);
        for(int it=0;it<20;++it){
            eps=std::clamp(eps,-0.999999*rho,0.999999*rho);
            const double root=std::sqrt(std::max(1.0e-30,rho*rho-eps*eps));
            const double arg=phase+k*eps;
            const double g=eps/root + A*k*std::sin(arg);
            const double gp=(rho*rho)/(root*root*root)+A*k*k*std::cos(arg);
            if(!std::isfinite(g)||!std::isfinite(gp)||std::abs(gp)<1.0e-18)break;
            const double step=g/gp;
            eps-=step;
            if(std::abs(step)<1.0e-15)break;
        }

        eps=std::clamp(eps,-0.999999*rho,0.999999*rho);
        previousEpsilon_=eps;
        const double root=std::sqrt(std::max(0.0,rho*rho-eps*eps));
        const double stylusRise=rho-root;
        const double y=A*std::cos(phase+k*eps)-stylusRise;
        return std::isfinite(y)?y:0.0;
    }

private:
    VinylTracingParameters p_{};
    double previousEpsilon_=0.0;
};

} // namespace MixEngine::V3Research
