#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace OverlapRemapMath {
using Real=long double;
struct P{Real x,y;};
using Quad=std::array<P,4>;
using Tri=std::array<P,3>;
inline P sub(P a,P b){return {a.x-b.x,a.y-b.y};}
inline Real cross(P a,P b){return a.x*b.y-a.y*b.x;}
inline Real orient(P a,P b,P c){return cross(sub(b,a),sub(c,a));}
inline Real area(const std::vector<P>&p){if(p.size()<3)return 0;Real a=0;for(size_t i=1;i+1<p.size();++i)a+=orient(p[0],p[i],p[i+1]);return a/2;}
inline Real area(const Quad&q){return (orient(q[0],q[1],q[2])+orient(q[0],q[2],q[3]))/2;}
inline std::array<Tri,2> triangulate(const Quad&q){
 if(orient(q[0],q[1],q[2])>0&&orient(q[0],q[2],q[3])>0)return {{{q[0],q[1],q[2]},{q[0],q[2],q[3]}}};
 if(orient(q[1],q[2],q[3])>0&&orient(q[1],q[3],q[0])>0)return {{{q[1],q[2],q[3]},{q[1],q[3],q[0]}}};
 throw std::runtime_error("Overlap remap requires simple positive quadrilateral");
}
inline Real intersect(const Tri&a,const Tri&b){std::vector<P> p(a.begin(),a.end()),out;
 for(int e=0;e<3&&!p.empty();++e){out.clear();P s=p.back();Real ds=orient(b[e],b[(e+1)%3],s);for(P z:p){Real dz=orient(b[e],b[(e+1)%3],z);if((ds>=0)!=(dz>=0)){const Real t=ds/(ds-dz);out.push_back({s.x+t*(z.x-s.x),s.y+t*(z.y-s.y)});}if(dz>=0)out.push_back(z);s=z;ds=dz;}p.swap(out);}
 return std::max(Real(0),area(p));
}
inline Real intersect(const std::array<Tri,2>&a,const std::array<Tri,2>&b){Real v=0;for(const auto&x:a)for(const auto&y:b)v+=intersect(x,y);return v;}
inline Real intersect(const Quad&a,const Quad&b){return intersect(triangulate(a),triangulate(b));}
// First moments relative to an arbitrary nearby origin reduce cancellation.
struct Moments{Real a=0,x=0,y=0;};
inline Moments moments(const std::vector<P>&p,P origin){Moments m;if(p.size()<3)return m;for(size_t i=1;i+1<p.size();++i){const Real a=orient(p[0],p[i],p[i+1])/2;m.a+=a;m.x+=a*((p[0].x-origin.x)+(p[i].x-origin.x)+(p[i+1].x-origin.x))/3;m.y+=a*((p[0].y-origin.y)+(p[i].y-origin.y)+(p[i+1].y-origin.y))/3;}return m;}
inline P centroid(const Quad&q){auto m=moments(std::vector<P>(q.begin(),q.end()),q[0]);if(!(m.a>0))throw std::runtime_error("Invalid centroid area");return {q[0].x+m.x/m.a,q[0].y+m.y/m.a};}
inline Moments intersection_moments(const Tri&a,const Tri&b,P origin){std::vector<P>p(a.begin(),a.end()),out;for(int e=0;e<3&&!p.empty();++e){out.clear();P s=p.back();Real ds=orient(b[e],b[(e+1)%3],s);for(P z:p){Real dz=orient(b[e],b[(e+1)%3],z);if((ds>=0)!=(dz>=0)){Real t=ds/(ds-dz);out.push_back({s.x+t*(z.x-s.x),s.y+t*(z.y-s.y)});}if(dz>=0)out.push_back(z);s=z;ds=dz;}p.swap(out);}auto m=moments(p,origin);if(m.a<=0)return {};return m;}
inline Moments intersection_moments(const std::array<Tri,2>&a,const std::array<Tri,2>&b,P origin){Moments m;for(const auto&x:a)for(const auto&y:b){auto p=intersection_moments(x,y,origin);m.a+=p.a;m.x+=p.x;m.y+=p.y;}return m;}
}
