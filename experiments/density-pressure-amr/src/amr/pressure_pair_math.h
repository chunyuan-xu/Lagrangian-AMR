#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

// Experimental AMR evidence on one shared face, not a reconstruction weight.
namespace PressurePairExperiment {
struct Flags { double refine=0.,retain=0.; };
inline int mode() {
    static const int v=std::getenv("AMR_PRESSURE_PAIR") ? std::atoi(std::getenv("AMR_PRESSURE_PAIR")) : 0;
    return v; // 0 off, 1 audit, 2 all levels, 3 finest transition only.
}
inline bool active(bool refining,int level,int maximum,int setting) {
    return setting==2 || (setting==3 && level==maximum-(refining?1:0));
}
inline double eta(double area,double rho,double gradient) {
    if(!std::isfinite(area)||area<=0.||!std::isfinite(rho)||!std::isfinite(gradient))
        return std::numeric_limits<double>::infinity();
    return std::sqrt(area)*std::abs(gradient)/std::max(std::abs(rho),1e-12);
}
inline Flags invalid() {
    const double inf=std::numeric_limits<double>::infinity();return {inf,inf};
}
inline Flags face(double pi,double pj,double ei,double ej,double pscale,
                  double rho_refine,double rho_retain,double p_refine,double p_retain) {
    if(!std::isfinite(pi)||!std::isfinite(pj)||!std::isfinite(ei)||!std::isfinite(ej)||
       !std::isfinite(pscale)||pscale<0.||ei<0.||ej<0.)return invalid();
    if(!(pi>pj))return {};
    const double jump=(pi-pj)/(std::abs(pi)+std::abs(pj)+pscale+1e-12);
    if(!std::isfinite(jump))return invalid();
    const double shared_density=std::max(ei,ej);
    return {double(shared_density>rho_refine && jump>p_refine),
            double(shared_density>=rho_retain && jump>p_retain)};
}
inline void merge(Flags &a,const Flags &b) {
    a.refine=std::max(a.refine,b.refine);a.retain=std::max(a.retain,b.retain);
}
}
