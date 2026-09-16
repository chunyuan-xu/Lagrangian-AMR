#pragma once
#include <cmath>
#include <cstdlib>
#include <limits>
#include "amr/least_squares_gradient.h"

// AMR-only common smoothness, not a WENO flux reconstruction.
namespace LinearServiceSensor {
inline int mode() {
    static const int v = std::getenv("AMR_WENO_MODE") ?
        std::atoi(std::getenv("AMR_WENO_MODE")) : 6;
    return v;
}
inline double difference(double a, double b) {
    if (!(a > 0.) || !(b > 0.) || !std::isfinite(a) || !std::isfinite(b))
        return std::numeric_limits<double>::quiet_NaN();
    return std::log(b) - std::log(a);
}
struct Fit {
    DensityLeastSquares::Fit density, pressure;
    void add(double dx, double dy, double ra, double rb, double pa, double pb,
             double pressure_scale=0.) {
        density.add(dx,dy,difference(ra,rb));
        if (!(pa>0.) || !(pb>0.) || !(pressure_scale>=0.) || !std::isfinite(pressure_scale))
            pressure.invalidate();
        else pressure.add(dx,dy,difference(pa+pressure_scale,pb+pressure_scale));
    }
    double indicator(double area, int route) const {
        const auto r=density.solve(), p=pressure.solve();
        if (!r.valid || !p.valid || !(area>0.) || !std::isfinite(area))
            return std::numeric_limits<double>::infinity();
        const double h=std::sqrt(area);
        // Constant-gamma rest-frame rho*p*fE scales as rho^(1/2)*p^(5/2).
        if (route==4) return h*std::hypot((r.x+5.*p.x)/6.,(r.y+5.*p.y)/6.);
        // Sum of linear log smoothness indicators: no cancellation and
        // no pressure AND gate suppressing density contacts.
        return h*std::hypot(r.magnitude,p.magnitude);
    }
};
}
