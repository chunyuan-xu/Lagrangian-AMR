#include <cassert>
#include <iostream>
#include "amr/linear_service_sensor.h"
using LinearServiceSensor::Fit;
Fit fit(double ax,double ay,double bx,double by,double units=1.) {
    Fit f;
    for (int i=0;i<4;++i) {
        const double x=i==0?.1:i==1?-.1:0.;
        const double y=i==2?.1:i==3?-.1:0.;
        f.add(x,y,units,units*std::exp(ax*x+ay*y),7.*units,7.*units*std::exp(bx*x+by*y));
    }
    return f;
}
int main() {
    assert(fit(0,0,0,0).indicator(.01,5)==0.);
    const auto a=fit(2,3,4,-1);
    assert(std::abs(a.indicator(.01,5)-.1*std::sqrt(30.))<1e-12);
    assert(std::abs(a.indicator(.01,4)-.1*std::hypot(22./6.,-2./6.))<1e-12);
    assert(std::abs(fit(2,3,4,-1,1e20).indicator(.01,5)-a.indicator(.01,5))<1e-12);
    assert(std::abs(a.indicator(.0025,5)*2-a.indicator(.01,5))<1e-12);
    assert(fit(3,0,0,0).indicator(.01,5)>.29); // pressure-constant contact
    assert(fit(5,0,-1,0).indicator(.01,4)<1e-12); // scalar cancellation
    assert(fit(5,0,-1,0).indicator(.01,5)>.5);   // shared beta avoids it
    Fit invalid; invalid.add(1,0,1,-1,1,1); invalid.add(0,1,1,1,1,1);
    assert(!std::isfinite(invalid.indicator(.01,5)));
    Fit cold,protected_cold,rescaled;
    for (int i=0;i<4;++i) {
        const double x=i==0?.1:i==1?-.1:0.,y=i==2?.1:i==3?-.1:0.;
        const double p=1e-8*std::exp(5*x);
        cold.add(x,y,1,1,1e-8,p);
        protected_cold.add(x,y,1,1,1e-8,p,1.);
        rescaled.add(x,y,1e5,1e5,1e2,p*1e10,1e10);
    }
    assert(cold.indicator(.01,5)>.49);
    assert(protected_cold.indicator(.01,6)<1e-7);
    assert(std::abs(protected_cold.indicator(.01,6)-rescaled.indicator(.01,6))<1e-12);
    std::cout << "linear-service: 12 manufactured checks passed\n";
}
