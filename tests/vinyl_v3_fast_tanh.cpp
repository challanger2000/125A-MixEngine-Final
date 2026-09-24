#include "../source/media_oversampling_live.h"
#include <algorithm>
#include <cmath>
#include <iostream>

int main(){
    double maxError=0.0;
    double minDerivative=1.0e9;
    double previous=MixEngine::vinylFastTanh(-1.25);
    bool monotonic=true;
    constexpr int steps=250000;
    for(int i=0;i<=steps;++i){
        const double x=-1.25+2.5*double(i)/double(steps);
        const double fast=MixEngine::vinylFastTanh(x);
        const double exact=std::tanh(x);
        maxError=std::max(maxError,std::abs(fast-exact));
        if(i>0){
            const double dx=2.5/double(steps);
            const double derivative=(fast-previous)/dx;
            minDerivative=std::min(minDerivative,derivative);
            if(fast<previous)monotonic=false;
        }
        previous=fast;
    }
    const double left=MixEngine::vinylFastTanh(1.25);
    const double right=std::tanh(1.25+1.0e-12);
    const double boundaryJump=std::abs(left-right);
    const double outer=MixEngine::vinylFastTanh(2.0);
    const double outerExact=std::tanh(2.0);

    std::cout<<"maxError="<<maxError
             <<" minDerivative="<<minDerivative
             <<" boundaryJump="<<boundaryJump
             <<" outerError="<<std::abs(outer-outerExact)
             <<" monotonic="<<(monotonic?"YES":"NO")<<"\n";

    const bool ok=maxError<1.0e-6
               && minDerivative>0.0
               && boundaryJump<1.1e-6
               && std::abs(outer-outerExact)<1.0e-15
               && monotonic;
    std::cout<<(ok?"PASS":"FAIL")<<": Vinyl fast tanh transfer parity\n";
    return ok?0:1;
}
