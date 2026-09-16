#pragma once
#include "amr/overlap_remap_math.h"
#include "amr/linear_remap_math.h"
#include "diagnostics/refresh_geometry_audit.h"
#include "physics/eos.h"
#include <limits>
// Isolated serial planar prototype: retain existing refinement, then remap
// post-refinement conserved states over the post-coarsen/refresh physical mesh.
// Mode 1 is read-only; mode 2 applies first-order conservative intersections.
// Mode 3 audits limited linear remap; mode 4 applies it (single tree only).
// No analytical Noh state, radius, clipping of fields, or AMR-tag modification.
namespace OverlapRemapExperiment {
using namespace OverlapRemapMath;
inline int mode(){static int m=std::getenv("AMR_OVERLAP_REMAP")?std::atoi(std::getenv("AMR_OVERLAP_REMAP")):0;return m;}
struct Cell{Quad p;std::array<Tri,2> triangles;Real x0,x1,y0,y1,V,m,E,g;P momentum;P center{};int64_t qx=0,qy=0,length=0;};
inline Cell cell(const CVariable&v){Cell c;const int order[4]={quad_data_t::LEFTBOTTOM,quad_data_t::RIGHTBOTTOM,quad_data_t::RIGHTUP,quad_data_t::LEFTUP};c.x0=c.y0=std::numeric_limits<Real>::infinity();c.x1=c.y1=-c.x0;for(int j=0;j<4;++j){auto x=v.corner_vector(idcnCoords_cur,order[j]);c.p[j]={x.x,x.y};c.x0=std::min(c.x0,c.p[j].x);c.x1=std::max(c.x1,c.p[j].x);c.y0=std::min(c.y0,c.p[j].y);c.y1=std::max(c.y1,c.p[j].y);}c.triangles=triangulate(c.p);c.V=v.cell(idVolume);c.m=v.cell(idMass);c.E=c.m*v.cell(idTotalEnergy_cur);auto u=v.cell_vector(idCentroidVelo_cur);c.momentum={c.m*u.x,c.m*u.y};c.g=v.cell(idGamma);SC_CHECK_ABORT(c.V>0&&std::abs(area(c.p)-c.V)<1e-10L*c.V,"Stored volume differs from remap polygon");return c;}
struct State{std::vector<Cell>old;bool merged=false;};
inline State&state(){static State s;return s;}
inline void capture(p4est_t*f){if(!mode())return;const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;SC_CHECK_ABORT(mode()>=1&&mode()<=4&&f->mpisize==1&&r.coord_type==p4est_data_t::MyCoordType::plane&&r.Scheme_type==p4est_data_t::MySchemeType::ControlVolume,"Overlap remap requires serial planar CV");if(mode()>=3)SC_CHECK_ABORT(f->connectivity->num_trees==1,"Linear remap prototype requires single tree");auto&s=state();SC_CHECK_ABORT(s.old.empty(),"Unfinished overlap pass");s.merged=false;RefreshGeometryAudit::each(f,[&](p4est_topidx_t,p4est_quadrant_t*q){auto c=cell(static_cast<quad_data_t*>(q->p.user_data)->m_vara);if(mode()>=3){c.center=centroid(c.p);c.qx=q->x;c.qy=q->y;c.length=P4EST_QUADRANT_LEN(q->level);}s.old.push_back(c);});}
inline void after_coarsen(p4est_t*f){if(mode())state().merged=(size_t)f->local_num_quadrants<state().old.size();}
struct Link{size_t target,donor;Real a,mx=0,my=0;};
inline LinearRemapMath::Values densities(const Cell&c){return {{c.m/c.V,c.momentum.x/c.V,c.momentum.y/c.V,c.E/c.V}};}
inline bool face_neighbors(const Cell&a,const Cell&b){return ((a.qx+a.length==b.qx||b.qx+b.length==a.qx)&&std::max(a.qy,b.qy)<std::min(a.qy+a.length,b.qy+b.length))||((a.qy+a.length==b.qy||b.qy+b.length==a.qy)&&std::max(a.qx,b.qx)<std::min(a.qx+a.length,b.qx+b.length));}
inline std::vector<LinearRemapMath::Fit> linear_fits(const std::vector<Cell>&old){
 std::vector<std::vector<LinearRemapMath::Sample>> n(old.size());
 for(size_t i=0;i<old.size();++i)for(size_t j=i+1;j<old.size();++j)if(face_neighbors(old[i],old[j])){n[i].push_back({old[j].center,densities(old[j])});n[j].push_back({old[i].center,densities(old[i])});}
 std::vector<LinearRemapMath::Fit>fits;fits.reserve(old.size());for(size_t i=0;i<old.size();++i){const auto&c=old[i];auto q=densities(c);auto fit=LinearRemapMath::fit(c.center,q,n[i]);LinearRemapMath::limit(fit,c.center,q,n[i],c.p,m_eps);fits.push_back(fit);}return fits;
}
inline void finish(p4est_t*f){if(!mode())return;auto&s=state();if(s.old.empty())return;if(!s.merged){s.old.clear();return;}
 std::vector<Cell>now;std::vector<CVariable*>vars;RefreshGeometryAudit::each(f,[&](p4est_topidx_t,p4est_quadrant_t*q){auto*v=&static_cast<quad_data_t*>(q->p.user_data)->m_vara;vars.push_back(v);now.push_back(cell(*v));});
 const size_t N=s.old.size(),M=now.size();Real xmin=s.old[0].x0,xmax=s.old[0].x1,ymin=s.old[0].y0,ymax=s.old[0].y1;for(const auto&c:s.old){xmin=std::min(xmin,c.x0);xmax=std::max(xmax,c.x1);ymin=std::min(ymin,c.y0);ymax=std::max(ymax,c.y1);SC_CHECK_ABORT(std::abs(c.g-s.old[0].g)<1e-14L,"Overlap remap requires common gamma");}
 const int B=std::max(1,(int)std::sqrt((double)N));std::vector<std::vector<size_t>> bins(B*B);auto ix=[&](Real x){return std::min(B-1,std::max(0,(int)((x-xmin)/(xmax-xmin)*B)));};auto iy=[&](Real y){return std::min(B-1,std::max(0,(int)((y-ymin)/(ymax-ymin)*B)));};
 for(size_t j=0;j<N;++j){const auto&c=s.old[j];for(int y=iy(c.y0);y<=iy(c.y1);++y)for(int x=ix(c.x0);x<=ix(c.x1);++x)bins[y*B+x].push_back(j);}
 std::vector<Real> rows(M,0),cols(N,0);std::vector<Link>links;std::vector<size_t>seen(N,M);size_t pairs=0;
 for(size_t i=0;i<M;++i){const auto&a=now[i];for(int y=iy(a.y0);y<=iy(a.y1);++y)for(int x=ix(a.x0);x<=ix(a.x1);++x)for(size_t j:bins[y*B+x]){if(seen[j]==i)continue;seen[j]=i;const auto&b=s.old[j];if(a.x0>=b.x1||b.x0>=a.x1||a.y0>=b.y1||b.y0>=a.y1)continue;++pairs;Moments moment;if(mode()>=3)moment=intersection_moments(a.triangles,b.triangles,b.center);const Real overlap=mode()>=3?moment.a:intersect(a.triangles,b.triangles);if(overlap>0){links.push_back({i,j,overlap,moment.x,moment.y});rows[i]+=overlap;cols[j]+=overlap;}}}
 Real maxrow=0,maxcol=0;for(size_t i=0;i<M;++i)maxrow=std::max(maxrow,std::abs(rows[i]/now[i].V-1));for(size_t j=0;j<N;++j)maxcol=std::max(maxcol,std::abs(cols[j]/s.old[j].V-1));
 const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;
 if(maxrow>=1e-9L||maxcol>=1e-9L)std::printf("OVERLAP_COVERAGE_FAILURE step=%d t=%.17g row=%.17g column=%.17g\n",r.current_step,r.current_time,(double)maxrow,(double)maxcol);
 SC_CHECK_ABORT(maxrow<1e-9L&&maxcol<1e-9L,"Old/new meshes fail bidirectional physical coverage");
 // Column normalization corrects only verified roundoff-size coverage errors.
 std::vector<std::array<Real,5>>out(M);std::array<Real,4>before{},after{},scale{};for(const auto&c:s.old){std::array<Real,4>q={c.m,c.momentum.x,c.momentum.y,c.E};for(int k=0;k<4;++k){before[k]+=q[k];scale[k]+=std::abs(q[k]);}}
 std::vector<LinearRemapMath::Fit>fits;std::vector<P>moment_sums(N,P{0,0});Real moment_residual=0,theta_sum=0,theta_min=1;size_t invalid=0,limited=0,positivity_limited=0;
 if(mode()>=3){fits=linear_fits(s.old);for(const auto&l:links){moment_sums[l.donor].x+=l.mx;moment_sums[l.donor].y+=l.my;}for(size_t j=0;j<N;++j){const auto&c=s.old[j];const auto&fit=fits[j];moment_residual=std::max(moment_residual,std::hypot(moment_sums[j].x,moment_sums[j].y)/(c.V*std::sqrt(c.V)));invalid+=!fit.valid;limited+=fit.theta<1;positivity_limited+=fit.positivity_limited;theta_sum+=fit.theta;theta_min=std::min(theta_min,fit.theta);}SC_CHECK_ABORT(moment_residual<1e-9L,"Donor first moments fail coverage");}
 for(const auto&l:links){const auto&c=s.old[l.donor];const Real w=l.a/cols[l.donor];LinearRemapMath::Values piece={{w*c.m,w*c.momentum.x,w*c.momentum.y,w*c.E}};
  if(mode()>=3){const auto&fit=fits[l.donor];const Real mx=l.mx-w*moment_sums[l.donor].x,my=l.my-w*moment_sums[l.donor].y;for(int k=0;k<4;++k)piece[k]+=(c.V/cols[l.donor])*fit.theta*(fit.g[k].x*mx+fit.g[k].y*my);auto average=piece;for(auto&q:average)q/=w*c.V;SC_CHECK_ABORT(LinearRemapMath::positive(average,0,m_eps),"Nonpositive limited overlap piece");}
  for(int k=0;k<4;++k){out[l.target][k]+=piece[k];}
  out[l.target][4]+=w*c.V;
 }
 if(mode()>=3)std::printf("LINEAR_REMAP step=%d t=%.17g mode=%d donors=%d invalid=%d limited=%d positivity_limited=%d theta_mean=%.17g theta_min=%.17g moment_residual=%.17g\n",r.current_step,r.current_time,mode(),(int)N,(int)invalid,(int)limited,(int)positivity_limited,(double)(theta_sum/N),(double)theta_min,(double)moment_residual);
 Real uniform=0,minie=std::numeric_limits<Real>::infinity(),maxdrho=0;size_t changed=0;
 for(size_t i=0;i<M;++i){const auto&q=out[i];for(int k=0;k<4;++k)after[k]+=q[k];uniform=std::max(uniform,std::abs(q[4]/now[i].V-1));SC_CHECK_ABORT(q[0]>0,"Nonpositive remapped mass");const Real ux=q[1]/q[0],uy=q[2]/q[0],E=q[3]/q[0],e=E-(ux*ux+uy*uy)/2;minie=std::min(minie,e);SC_CHECK_ABORT(std::isfinite(e)&&e>m_eps,"Nonpositive remapped internal energy");const Real rho=q[0]/now[i].V,drho=std::abs(rho-vars[i]->cell(idDensity_cur));maxdrho=std::max(maxdrho,drho);changed+=drho>1e-10L*std::max(Real(1),rho);
  if(mode()==2||mode()==4){auto&v=*vars[i];v.cell(idMass)=(double)q[0];for(auto id:{idDensity_cur,idDensity_half,idDensity_lag})v.cell(id)=(double)rho;for(auto id:{idCentroidVelo_cur,idCentroidVelo_half,idCentroidVelo_lag})v.cell_vector(id)=CDoubleVector((double)ux,(double)uy);for(auto id:{idTotalEnergy_cur,idTotalEnergy_half,idTotalEnergy_lag})v.cell(id)=(double)E;for(auto id:{idInternalEnergy_cur,idInternalEnergy_half,idInternalEnergy_lag})v.cell(id)=(double)e;const double p=PhysicalAlg::EquationOfState(v.cell(idGamma),(double)rho,(double)e);for(auto id:{idPressure_cur,idPressure_half,idPressure_lag})v.cell(id)=p;v.cell(idSoundSpeed)=PhysicalAlg::CalculateSoundSpeed(v.cell(idGamma),p,(double)rho);}
 }
 Real conservation=0;for(int k=0;k<4;++k)conservation=std::max(conservation,std::abs(after[k]-before[k])/std::max(scale[k],1e-300L));SC_CHECK_ABORT(conservation<1e-12L&&uniform<1e-9L,"Overlap remap failed conservation or constant preservation");
 // Check stored double states independently when applying the remap.
 Real stored=0;if(mode()==2||mode()==4){std::array<Real,4>tot{};for(auto*v:vars){Real m=v->cell(idMass);auto u=v->cell_vector(idCentroidVelo_cur);tot[0]+=m;tot[1]+=m*u.x;tot[2]+=m*u.y;tot[3]+=m*v->cell(idTotalEnergy_cur);}for(int k=0;k<4;++k)stored=std::max(stored,std::abs(tot[k]-before[k])/std::max(scale[k],1e-300L));SC_CHECK_ABORT(stored<1e-12L,"Stored overlap states fail conservation");}
 std::printf("OVERLAP_REMAP step=%d t=%.17g mode=%d old=%d new=%d pairs=%d links=%d row_residual=%.17g col_residual=%.17g conservation=%.17g stored_conservation=%.17g uniform_residual=%.17g min_ie=%.17g changed=%d max_delta_rho=%.17g\n",r.current_step,r.current_time,mode(),(int)N,(int)M,(int)pairs,(int)links.size(),(double)maxrow,(double)maxcol,(double)conservation,(double)stored,(double)uniform,(double)minie,(int)changed,(double)maxdrho);s.old.clear();s.merged=false;
}
}
