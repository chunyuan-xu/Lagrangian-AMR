#pragma once
#include "overlap_remap_math.h"
#include "least_squares_gradient.h"
namespace LinearRemapMath {
using namespace OverlapRemapMath;
using Values=std::array<Real,4>;
using Gradient=std::array<P,4>;
struct Sample{P x;Values q;};
struct Fit{Gradient g{};Real theta=0;bool valid=false;bool positivity_limited=false;};
inline Real thermal(const Values&q){return q[3]-(q[1]*q[1]+q[2]*q[2])/(2*q[0]);}
inline Values eval(const Values&q,const Gradient&g,P d,Real theta){Values v;for(int k=0;k<4;++k)v[k]=q[k]+theta*(g[k].x*d.x+g[k].y*d.y);return v;}
inline bool positive(const Values&q,Real rho_floor,Real e_floor){return q[0]>=rho_floor&&std::isfinite(q[3])&&thermal(q)>=q[0]*e_floor;}
inline Fit fit(P center,const Values&q,const std::vector<Sample>&n){Fit r;for(int k=0;k<4;++k){DensityLeastSquares::Fit f;for(const auto&s:n)f.add((double)(s.x.x-center.x),(double)(s.x.y-center.y),(double)(s.q[k]-q[k]));auto a=f.solve();if(!a.valid)return {};r.g[k]={a.x,a.y};}r.valid=true;r.theta=1;return r;}
// One common factor limits all four conservative gradients. Linear mean is
// preserved exactly because the expansion uses the physical area centroid.
inline void limit(Fit&r,P center,const Values&q,const std::vector<Sample>&n,const Quad&vertices,Real minimum_e){if(!r.valid){r.theta=0;return;}Values lo=q,hi=q;for(const auto&s:n)for(int k=0;k<4;++k){lo[k]=std::min(lo[k],s.q[k]);hi[k]=std::max(hi[k],s.q[k]);}Real theta=1;for(P x:vertices){P d=sub(x,center);for(int k=0;k<4;++k){Real dq=r.g[k].x*d.x+r.g[k].y*d.y;if(dq>0)theta=std::min(theta,(hi[k]-q[k])/dq);else if(dq<0)theta=std::min(theta,(lo[k]-q[k])/dq);}}
 theta=std::max(Real(0),std::min(Real(1),theta));const Real rho_floor=q[0]*1e-12L,e_floor=std::max(minimum_e,thermal(q)/q[0]*1e-10L);if(!positive(q,rho_floor,e_floor))throw std::runtime_error("Invalid mean state for remap limiter");
 for(P x:vertices){P d=sub(x,center);if(positive(eval(q,r.g,d,theta),rho_floor,e_floor))continue;r.positivity_limited=true;Real a=0,b=theta;for(int k=0;k<70;++k){Real mid=(a+b)/2;if(positive(eval(q,r.g,d,mid),rho_floor,e_floor))a=mid;else b=mid;}theta=a*(1-1e-12L);}
 r.theta=theta;for(P x:vertices)if(!positive(eval(q,r.g,sub(x,center),theta),rho_floor,e_floor))throw std::runtime_error("Remap vertex positivity check failed");
}
}
