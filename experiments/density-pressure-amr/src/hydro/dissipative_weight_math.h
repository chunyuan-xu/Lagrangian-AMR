#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
// Pure three-cell capped-simplex projection. Not a fully discrete entropy or
// positivity guarantee. Infeasible scalar-weight cases leave the input intact.
namespace DissipativeWeightMath {
using A=std::array<double,3>;
struct Result { A weights,cap;bool valid=false,feasible=false,changed=false; };
inline Result project(const A&base,const A&d,const A&q){
 Result r;r.weights=base;r.cap={{0,0,0}};double total=0,capacity=0;
 for(int i=0;i<3;++i){if(!(base[i]>0)||!std::isfinite(base[i])||!(d[i]>=0)||!std::isfinite(d[i])||!std::isfinite(q[i]))return r;total+=base[i];}
 if(!std::isfinite(total)||std::abs(total-1)>1e-12)return r;
 r.valid=true;
 for(int i=0;i<3;++i){r.cap[i]=q[i]>0?std::min(1.,d[i]/q[i]):1.;capacity+=r.cap[i];}
 if(capacity<1)return r;r.feasible=true;
 bool needed=false;for(int i=0;i<3;++i)if(base[i]>r.cap[i])needed=true;
 if(!needed)return r;
 A w={{0,0,0}};std::array<bool,3> free={{true,true,true}};double remaining=1.;
 for(int pass=0;pass<3;++pass){double bsum=0;for(int i=0;i<3;++i)if(free[i])bsum+=base[i];if(bsum==0)break;
  bool capped=false;A trial={{0,0,0}};
  for(int i=0;i<3;++i)if(free[i])trial[i]=remaining*base[i]/bsum;
  for(int i=0;i<3;++i)if(free[i]&&trial[i]>r.cap[i]){w[i]=r.cap[i];remaining-=w[i];free[i]=false;capped=true;}
  if(!capped){for(int i=0;i<3;++i)if(free[i])w[i]=trial[i];break;}
 }
 const double eps=64*std::numeric_limits<double>::epsilon();double sum=0;
 for(int i=0;i<3;++i){if(!std::isfinite(w[i])||w[i]<-eps||w[i]>r.cap[i]+eps){r.valid=false;return r;}sum+=w[i];}
 if(std::abs(sum-1)>eps){r.valid=false;return r;}r.weights=w;r.changed=true;return r;
}
}

