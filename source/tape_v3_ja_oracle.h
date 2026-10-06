#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

namespace MixEngine::V3Research {

struct JilesAthertonParameters {
    double Ms=3.5e5;
    double k=27.0e3;
    double a=22.0e3;
    double c=1.7e-1;
    double alpha=1.6e-3;
};

inline double jaLangevin(double x) noexcept {
    const double ax=std::abs(x);
    if(ax<1.0e-4) return x/3.0;
    const double th=std::tanh(x);
    if(std::abs(th)<1.0e-15) return x/3.0;
    return 1.0/th-1.0/x;
}

inline double jaLangevinDerivative(double x) noexcept {
    const double ax=std::abs(x);
    if(ax<1.0e-4) return 1.0/3.0;
    const double th=std::tanh(x);
    if(std::abs(th)<1.0e-15) return 1.0/3.0;
    const double coth=1.0/th;
    return 1.0/(x*x)-coth*coth+1.0;
}

class JilesAthertonOracle {
public:
    void reset() noexcept { M_=0.0; HPrev_=0.0; HDotPrev_=0.0; initialised_=false; }

    double processField(double H,double sampleRate) noexcept {
        if(!(sampleRate>0.0)||!std::isfinite(H)) return std::numeric_limits<double>::quiet_NaN();
        if(!initialised_){
            HPrev_=H; HDotPrev_=0.0; M_=0.0; initialised_=true;
            return M_;
        }

        const double T=1.0/sampleRate;
        const double HDot=2.0*(H-HPrev_)/T-HDotPrev_;
        const double HMid=0.5*(HPrev_+H);
        const double HDotMid=0.5*(HDotPrev_+HDot);

        const double k1=T*dMdt(M_,HPrev_,HDotPrev_);
        const double k2=T*dMdt(M_+0.5*k1,HMid,HDotMid);
        const double k3=T*dMdt(M_+0.5*k2,HMid,HDotMid);
        const double k4=T*dMdt(M_+k3,H,HDot);
        const double next=M_+(k1+2.0*k2+2.0*k3+k4)/6.0;

        HPrev_=H;
        HDotPrev_=HDot;
        M_=std::isfinite(next)?next:std::numeric_limits<double>::quiet_NaN();
        return M_;
    }

    double magnetisation() const noexcept { return M_; }
    const JilesAthertonParameters& parameters() const noexcept { return p_; }

private:
    double dMdt(double M,double H,double HDot) const noexcept {
        const double Q=(H+p_.alpha*M)/p_.a;
        const double L=jaLangevin(Q);
        const double Lp=jaLangevinDerivative(Q);
        const double Man=p_.Ms*L;
        const double diff=Man-M;
        const double deltaS=HDot>=0.0?1.0:-1.0;
        const double deltaM=(deltaS*diff>0.0)?1.0:0.0;

        const double d1=(1.0-p_.c)*deltaS*p_.k-p_.alpha*diff;
        double irreversible=0.0;
        if(std::abs(d1)>1.0e-12)
            irreversible=((1.0-p_.c)*deltaM*diff/d1)*HDot;

        const double reversible=p_.c*(p_.Ms/p_.a)*HDot*Lp;
        const double d2=1.0-p_.c*p_.alpha*(p_.Ms/p_.a)*Lp;
        if(std::abs(d2)<1.0e-12) return 0.0;
        return (irreversible+reversible)/d2;
    }

    JilesAthertonParameters p_{};
    double M_=0.0;
    double HPrev_=0.0;
    double HDotPrev_=0.0;
    bool initialised_=false;
};

} // namespace MixEngine::V3Research
