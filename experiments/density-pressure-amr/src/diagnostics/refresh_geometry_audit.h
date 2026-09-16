#pragma once
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "defines.h"
#include "variable.h"
// Read-only geometry/thermodynamics audit. No analytical shock input.
namespace RefreshGeometryAudit {
inline bool enabled(){return std::getenv("AMR_REFRESH_GEOMETRY_AUDIT")!=nullptr;}
template<class F> inline void each(p4est_t*f,F fn){for(p4est_topidx_t ti=f->first_local_tree;ti<=f->last_local_tree;++ti){auto*t=p4est_tree_array_index(f->trees,ti);for(size_t k=0;k<t->quadrants.elem_count;++k)fn(ti,p4est_quadrant_array_index(&t->quadrants,k));}}
inline void volume(p4est_t*f,const char*phase){if(!enabled())return;long double a=0,m=0;each(f,[&](p4est_topidx_t,p4est_quadrant_t*q){const auto&v=static_cast<quad_data_t*>(q->p.user_data)->m_vara;a+=v.cell(idVolume);m+=v.cell(idMass);});const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;std::printf("GEOMETRY_VOLUME step=%d t=%.17g phase=%s volume=%.17g mass=%.17g n=%d\n",r.current_step,r.current_time,phase,(double)a,(double)m,(int)f->local_num_quadrants);}
struct Old{int x,y,level;double V,m,E,e,p,g;CDoubleVector u;};
inline std::vector<Old>&old(){static std::vector<Old>s;return s;}
inline void capture(p4est_t*f){if(!enabled())return;SC_CHECK_ABORT(f->mpisize==1,"Refresh geometry audit is serial");old().clear();each(f,[&](p4est_topidx_t,p4est_quadrant_t*q){const auto&v=static_cast<quad_data_t*>(q->p.user_data)->m_vara;old().push_back({q->x,q->y,q->level,v.cell(idVolume),v.cell(idMass),v.cell(idTotalEnergy_cur),v.cell(idInternalEnergy_cur),v.cell(idPressure_cur),v.cell(idGamma),v.cell_vector(idCentroidVelo_cur)});});volume(f,"before_refresh");}
inline void finish(p4est_t*f){if(!enabled())return;const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;size_t k=0;each(f,[&](p4est_topidx_t ti,p4est_quadrant_t*q){const auto&v=static_cast<quad_data_t*>(q->p.user_data)->m_vara;SC_CHECK_ABORT(k<old().size(),"Refresh changed topology");const auto&o=old()[k++];const auto u=v.cell_vector(idCentroidVelo_cur),x=v.cell_vector(idCentroidCoord_cur);SC_CHECK_ABORT(q->x==o.x&&q->y==o.y&&q->level==o.level,"Refresh changed leaf order");SC_CHECK_ABORT(v.cell(idMass)==o.m&&v.cell(idTotalEnergy_cur)==o.E&&v.cell(idInternalEnergy_cur)==o.e&&u.x==o.u.x&&u.y==o.u.y,"Refresh changed conservative state");const double V=v.cell(idVolume),dp=(o.g-1)*o.m*o.e*(1/V-1/o.V),pEOSold=(o.g-1)*o.m*o.e/o.V,pEOSnew=(o.g-1)*o.m*o.e/V;SC_CHECK_ABORT(V>0&&o.V>0,"Invalid refresh volume");
 if(std::abs(V/o.V-1)>1e-12)std::printf("REFRESH_GEOMETRY step=%d t=%.17g tree=%d level=%d x=%d y=%d radius=%.17g old_volume=%.17g new_volume=%.17g volume_ratio=%.17g mass=%.17g old_pressure_eos=%.17g new_pressure_eos=%.17g dp_geometry=%.17g pressure_sync_residual=%.17g uniform_density_ratio=%.17g\n",r.current_step,r.current_time,(int)ti,q->level,q->x,q->y,std::hypot(x.x,x.y),o.V,V,V/o.V,o.m,pEOSold,pEOSnew,dp,v.cell(idPressure_cur)-pEOSnew,o.V/V);});SC_CHECK_ABORT(k==old().size(),"Refresh changed leaf count");volume(f,"after_refresh");old().clear();}
}
