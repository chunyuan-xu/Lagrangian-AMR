#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "defines.h"
#include "variable.h"
// Independent read-only sum from actual forces and half/lag conservative states.
// Physical-boundary nodes are identified by logical coordinates, not the Noh radius.
namespace HydroBudgetAudit {
inline bool enabled(){return std::getenv("AMR_TRANSFER_ENERGY_AUDIT")!=nullptr;}
inline void record(p4est_t*f){
 if(!enabled())return;const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;
 SC_CHECK_ABORT(f->mpisize==1&&f->connectivity->num_trees==1&&r.coord_type==p4est_data_t::MyCoordType::plane&&r.Scheme_type==p4est_data_t::MySchemeType::ControlVolume,"Budget audit requires serial single-tree planar CV");
 long double fx=0,fy=0,bx=0,by=0,work=0,bwork=0,dmx=0,dmy=0,dE=0,fscale=0,wscale=0,mscale=0,escale=0,stored_work=0;
 auto*t=p4est_tree_array_index(f->trees,0);
 for(size_t k=0;k<t->quadrants.elem_count;++k){auto*q=p4est_quadrant_array_index(&t->quadrants,k);const auto*data=static_cast<quad_data_t*>(q->p.user_data);const auto&v=data->m_vara;const long long h=P4EST_QUADRANT_LEN(q->level);const double m=v.cell(idMass);const auto uh=v.cell_vector(idCentroidVelo_half),un=v.cell_vector(idCentroidVelo_lag);
  dmx+=(long double)m*(un.x-uh.x);dmy+=(long double)m*(un.y-uh.y);dE+=(long double)m*(v.cell(idTotalEnergy_lag)-v.cell(idTotalEnergy_half));mscale+=(long double)m*(std::hypot(un.x,un.y)+std::hypot(uh.x,uh.y));escale+=(long double)m*(std::abs(v.cell(idTotalEnergy_half))+std::abs(v.cell(idTotalEnergy_lag)));stored_work+=v.cell(idTotalWork);
  auto add=[&](CDoubleVector F,CDoubleVector U,bool boundary){fx+=F.x;fy+=F.y;const long double w=(long double)U.x*F.x+(long double)U.y*F.y;work+=w;fscale+=std::hypot(F.x,F.y);wscale+=std::abs(w);if(boundary){bx+=F.x;by+=F.y;bwork+=w;}};
  for(int c=0;c<4;++c){const bool right=c==quad_data_t::RIGHTBOTTOM||c==quad_data_t::RIGHTUP,up=c==quad_data_t::LEFTUP||c==quad_data_t::RIGHTUP;const long long x=q->x+(right?h:0),y=q->y+(up?h:0);const bool boundary=x==0||y==0||x==P4EST_ROOT_LEN||y==P4EST_ROOT_LEN;
   add(v.corner_vector(idcnFcp,c)+v.corner_vector(idcnFluxRelaxed,c),v.corner_vector(idcnVelocity_lag,c),boundary);
   const auto&pc=data->m_pc_edge_data[c];if(pc.IsParentChildBoun){const bool outside=(c==quad_data_t::LEFT&&q->x==0)||(c==quad_data_t::RIGHT&&q->x+h==P4EST_ROOT_LEN)||(c==quad_data_t::BOTTOM&&q->y==0)||(c==quad_data_t::UP&&q->y+h==P4EST_ROOT_LEN);SC_CHECK_ABORT(!outside,"Parent-child force is internal");add(v.corner_vector(ideFcp,c)+pc.FluxRelaxed,pc.Hanging_velocity,false);}
  }
 }
 const long double dt=r.dt_iter;const double force_rel=(double)(std::hypot(fx-bx,fy-by)/std::max(fscale,1e-300L)),work_rel=(double)(std::abs(work-bwork)/std::max(wscale,1e-300L));
 const double momentum_rel=(double)(std::hypot(dmx+dt*bx,dmy+dt*by)/std::max(mscale+dt*fscale,1e-300L));const double energy_rel=(double)(std::abs(dE+dt*bwork)/std::max(escale+dt*wscale,1e-300L));const double stored_rel=(double)(std::abs(stored_work-work)/std::max(wscale,1e-300L));
 std::printf("HYDRO_BUDGET step=%d t=%.17g dt=%.17g force_rel=%.17g work_rel=%.17g momentum_rel=%.17g energy_rel=%.17g stored_work_rel=%.17g boundary_fx=%.17g boundary_fy=%.17g boundary_work=%.17g delta_mx=%.17g delta_my=%.17g delta_energy=%.17g\n",r.current_step,r.current_time,r.dt_iter,force_rel,work_rel,momentum_rel,energy_rel,stored_rel,(double)bx,(double)by,(double)bwork,(double)dmx,(double)dmy,(double)dE);
 SC_CHECK_ABORT(std::isfinite(force_rel)&&std::isfinite(work_rel)&&std::isfinite(momentum_rel)&&std::isfinite(energy_rel)&&force_rel<1e-10&&work_rel<1e-10&&momentum_rel<1e-10&&energy_rel<1e-10&&stored_rel<1e-10,"Independent hydro budget failed");
}
}

