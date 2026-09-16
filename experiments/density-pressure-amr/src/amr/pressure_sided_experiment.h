#pragma once
#include <cmath>
#include <cstdlib>
#include <limits>

// Isolated sensor experiment, not a flux/reconstruction weight.  No analytic
// radius, exact shock pressure, cell level or diagnostic distance is used.
namespace PressureSidedExperiment {
inline int mode() {
    static const int value=std::getenv("AMR_PRESSURE_SIDED") ?
        std::atoi(std::getenv("AMR_PRESSURE_SIDED")) : 0;
    return value; // 0 off, 1 audit, 2 all levels, 3 finest transitions only.
}
inline double select(double symmetric, double sided, bool refining,
                     int level, int maximum_level, int setting) {
    return (setting==2 || (setting==3 &&
        level==maximum_level-(refining?1:0))) ? sided : symmetric;
}
inline double high_side(double target, double neighbor, double symmetric) {
    if(!std::isfinite(target) || !std::isfinite(neighbor) ||
       !std::isfinite(symmetric) || symmetric<0.)
        return std::numeric_limits<double>::infinity();
    return target>neighbor ? symmetric : 0.;
}
}
