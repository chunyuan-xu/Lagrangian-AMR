#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
// Exact active-set enumeration for 3 half-space constraints after eliminating
// the two shared-force equality constraints. No hydrodynamic state is changed here.
namespace DissipativeVectorMath {
struct V{double x=0,y=0;};
inline V add(V a,V b){return {a.x+b.x,a.y+b.y};}
inline V sub(V a,V b){return {a.x-b.x,a.y-b.y};}
inline V mul(double s,V a){return {s*a.x,s*a.y};}
inline double dot(V a,V b){return a.x*b.x+a.y*b.y;}
inline double norm(V a){return std::hypot(a.x,a.y);}
using A=std::array<double,3>;using VA=std::array<V,3>;
struct Result{VA force;A multiplier={{0,0,0}};bool valid=false,feasible=false,changed=false;double cost=0;};
inline bool solve(double h[3][3],const A&g,int mask,A&lambda){
 int ids[3],n=0;for(int i=0;i<3;++i)if(mask&(1<<i))ids[n++]=i;
 double a[3][4]={{0}},s[3]={};
 for(int i=0;i<n;++i){if(!(h[ids[i]][ids[i]]>0))return false;s[i]=std::sqrt(h[ids[i]][ids[i]]);}
 for(int i=0;i<n;++i){for(int j=0;j<n;++j)a[i][j]=h[ids[i]][ids[j]]/(s[i]*s[j]);a[i][n]=g[ids[i]]/s[i];}
 for(int j=0;j<n;++j){int pivot=j;for(int i=j+1;i<n;++i)if(std::abs(a[i][j])>std::abs(a[pivot][j]))pivot=i;
  if(std::abs(a[pivot][j])<1e-12)return false;for(int k=j;k<=n;++k)std::swap(a[j][k],a[pivot][k]);
  const double q=a[j][j];for(int k=j;k<=n;++k)a[j][k]/=q;
  for(int i=0;i<n;++i)if(i!=j){const double q2=a[i][j];for(int k=j;k<=n;++k)a[i][k]-=q2*a[j][k];}
 }
 lambda={{0,0,0}};for(int i=0;i<n;++i)lambda[ids[i]]=a[i][n]/s[i];return true;
}
inline Result project(const A&w,const VA&du,const A&D,V R){
 Result out;double sum=0;A upper={{0,0,0}},g={{0,0,0}};VA n;double scale=norm(R);
 for(int i=0;i<3;++i){out.force[i]=mul(w[i],R);sum+=w[i];if(!(w[i]>0)||!std::isfinite(w[i])||!(D[i]>=0)||!std::isfinite(D[i])||!std::isfinite(norm(du[i])))return out;
  const double length=norm(du[i]);n[i]=length>0?mul(1/length,du[i]):V{};upper[i]=length>0?D[i]/length:0;scale=std::max(scale,upper[i]);g[i]=dot(n[i],out.force[i])-upper[i];}
 if(!std::isfinite(scale)||std::abs(sum-1)>1e-12)return out;out.valid=true;scale=std::max(scale,1e-300);const double tol=2e-12*scale;
 if(g[0]<=0&&g[1]<=0&&g[2]<=0){out.feasible=true;return out;}
 double h[3][3];for(int i=0;i<3;++i)for(int j=0;j<3;++j)h[i][j]=(i==j?w[i]*dot(n[i],n[i]):0)-w[i]*w[j]*dot(n[i],n[j]);
 double best=std::numeric_limits<double>::infinity();
 for(int mask=1;mask<8;++mask){A lambda;if(!solve(h,g,mask,lambda))continue;bool good=true;for(double l:lambda)if(l<0||!std::isfinite(l))good=false;if(!good)continue;
  V Amean;for(int i=0;i<3;++i)Amean=add(Amean,mul(w[i]*lambda[i],n[i]));VA f;V fsum;double cost=0;
  for(int i=0;i<3;++i){const V delta=mul(w[i],sub(Amean,mul(lambda[i],n[i])));f[i]=add(mul(w[i],R),delta);fsum=add(fsum,f[i]);cost+=dot(delta,delta)/(2*w[i]);if(dot(n[i],f[i])-upper[i]>tol)good=false;}
  if(norm(sub(fsum,R))>tol||!std::isfinite(cost))good=false;
  if(good&&cost<best){best=cost;out.force=f;out.multiplier=lambda;out.feasible=true;out.changed=true;out.cost=cost;}
 }
 return out;
}
}

