#pragma once
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
// Geometry-only experiment. Vertices must be cyclic, either orientation.
// For a rectangle and a normal parallel to a side this returns that side length.
namespace DirectionalLengthExperiment {
inline bool enabled() {
 static const bool value=[](){const char*s=std::getenv("AMR_DIRECTIONAL_LENGTH");return s&&std::atoi(s)==1;}();
 return value;
}
inline double length(const std::array<std::array<double,2>,4>&x,
                     double area,double gx,double gy) {
 const double invalid=std::numeric_limits<double>::quiet_NaN();
 const double g=std::hypot(gx,gy);
 if(!(area>0.)||!std::isfinite(area)||!std::isfinite(g))return invalid;
 if(g==0.)return std::sqrt(area);
 const double nx=gx/g,ny=gy/g;double projected_perimeter=0.;
 for(int k=0;k<4;++k){int j=(k+1)%4;double dx=x[j][0]-x[k][0],dy=x[j][1]-x[k][1];
  if(!std::isfinite(dx)||!std::isfinite(dy)||!(std::hypot(dx,dy)>0.))return invalid;
  projected_perimeter+=std::abs(nx*dy-ny*dx);
 }
 if(!(projected_perimeter>0.)||!std::isfinite(projected_perimeter))return invalid;
 return 2.*area/projected_perimeter;
}
}
