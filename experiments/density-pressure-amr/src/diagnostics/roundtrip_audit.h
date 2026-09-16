#pragma once
#include <array>
#include <cmath>
#include "defines.h"
#include "variable.h"
#include "amr/coarsen_plan_experiment.h"
namespace RoundtripAudit {
using Key=CoarsenPlanExperiment::Key;
struct State{double rho,p,m,volume,e,kin,total;std::array<double,8>xy;};
inline State state(const CVariable&v){
 const auto u=v.cell_vector(idCentroidVelo_cur);
 State s{v.cell(idDensity_cur),v.cell(idPressure_cur),v.cell(idMass),v.cell(idVolume),v.cell(idInternalEnergy_cur),.5*(u.x*u.x+u.y*u.y),v.cell(idTotalEnergy_cur),{}};
 for(int c=0;c<4;++c){auto x=v.corner_vector(idcnCoords_cur,c);s.xy[2*c]=x.x;s.xy[2*c+1]=x.y;}return s;
}
using Snapshot=std::map<Key,State>;
inline Snapshot capture(const CoarsenPlanExperiment::Plan&p){
 Snapshot old;for(auto &a:p.leaves){auto*q=a.second;if(q->level==0)continue;p4est_quadrant_t parent;p4est_quadrant_parent(q,&parent);if(p.blocked.count(CoarsenPlanExperiment::key(std::get<0>(a.first),&parent)))old.emplace(a.first,state(static_cast<quad_data_t*>(q->p.user_data)->m_vara));}return old;
}
inline void compare(p4est_t*f,const Snapshot&old){
 double drho=0.,dp=0.,dv=0.,de=0.,dx=0.,mass0=0.,mass1=0.,ie0=0.,ie1=0.,ke0=0.,ke1=0.,te0=0.,te1=0.;int matched=0;
 for(p4est_topidx_t t=f->first_local_tree;t<=f->last_local_tree;++t){auto*tr=p4est_tree_array_index(f->trees,t);for(size_t k=0;k<tr->quadrants.elem_count;++k){auto*q=p4est_quadrant_array_index(&tr->quadrants,k);auto it=old.find(CoarsenPlanExperiment::key(t,q));if(it==old.end())continue;++matched;const auto&a=it->second;const auto b=state(static_cast<quad_data_t*>(q->p.user_data)->m_vara);
 drho=std::max(drho,std::abs(b.rho-a.rho)/std::max(std::abs(a.rho),1e-12));dp=std::max(dp,std::abs(b.p-a.p));dv=std::max(dv,std::abs(b.volume-a.volume)/std::max(std::abs(a.volume),1e-30));de=std::max(de,std::abs(b.e-a.e));for(int c=0;c<8;++c)dx=std::max(dx,std::abs(b.xy[c]-a.xy[c]));
 mass0+=a.m;mass1+=b.m;ie0+=a.m*a.e;ie1+=b.m*b.e;ke0+=a.m*a.kin;ke1+=b.m*b.kin;te0+=a.m*a.total;te1+=b.m*b.total;}}
 const auto&run=static_cast<P4estBridge*>(f->user_pointer)->data;
 P4EST_GLOBAL_PRODUCTIONF("ROUNDTRIP step=%d t=%.17g mode=%d cells=%d missing=%d drho_rel=%.17g dp_abs=%.17g dvolume_rel=%.17g de_abs=%.17g dx_abs=%.17g dmass=%.17g die=%.17g dke=%.17g dtotal=%.17g\n",run.current_step,run.current_time,CoarsenPlanExperiment::mode(),matched,(int)old.size()-matched,drho,dp,dv,de,dx,mass1-mass0,ie1-ie0,ke1-ke0,te1-te0);
}
}
