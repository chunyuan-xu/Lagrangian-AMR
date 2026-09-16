#pragma once
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include "defines.h"
#include "variable.h"
namespace TransferEnergyAudit {
inline bool enabled(){static bool a=std::getenv("AMR_TRANSFER_ENERGY_AUDIT")!=nullptr;return a;}
inline void merge(p4est_t*f,p4est_topidx_t tree,p4est_quadrant_t*parent,p4est_quadrant_t**children,const char*stage){
 if(!enabled())return;SC_CHECK_ABORT(f->mpisize==1,"Transfer audit requires serial run");const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;const auto&p=static_cast<quad_data_t*>(parent->p.user_data)->m_vara;
 double m=0,I=0,K=0,E=0,V=0,pV=0,mx=0,my=0;const double gamma=p.cell(idGamma);
 for(int i=0;i<4;++i){const auto&v=static_cast<quad_data_t*>(children[i]->p.user_data)->m_vara;SC_CHECK_ABORT(std::abs(v.cell(idGamma)-gamma)<1e-14,"Transfer pressure decomposition requires common gamma");const double mass=v.cell(idMass);const auto u=v.cell_vector(idCentroidVelo_cur);m+=mass;I+=mass*v.cell(idInternalEnergy_cur);K+=.5*mass*(u.x*u.x+u.y*u.y);E+=mass*v.cell(idTotalEnergy_cur);V+=v.cell(idVolume);pV+=v.cell(idPressure_cur)*v.cell(idVolume);mx+=mass*u.x;my+=mass*u.y;}
 const double mp=p.cell(idMass),Vp=p.cell(idVolume),Ip=mp*p.cell(idInternalEnergy_cur),Ep=mp*p.cell(idTotalEnergy_cur);const auto u=p.cell_vector(idCentroidVelo_cur),x=p.cell_vector(idCentroidCoord_cur);const double Kp=.5*mp*(u.x*u.x+u.y*u.y),loss=K-Kp;
 const double pheat=(gamma-1)*loss/V,pgeometry=(gamma-1)*(I+loss)*(1/Vp-1/V),pEOS=(gamma-1)*Ip/Vp;
 std::printf("TRANSFER_ENERGY step=%d t=%.17g stage=%s tree=%d level=%d x=%d y=%d radius=%.17g mass=%.17g mass_residual=%.17g momentum_x_residual=%.17g momentum_y_residual=%.17g child_internal=%.17g parent_internal=%.17g child_kinetic=%.17g parent_kinetic=%.17g kinetic_loss=%.17g thermal_identity_residual=%.17g energy_residual=%.17g child_volume=%.17g parent_volume=%.17g volume_ratio=%.17g rho_parent=%.17g p_child_avg=%.17g p_parent_eos=%.17g p_parent_stored=%.17g dp_heat=%.17g dp_geometry=%.17g dp_decomp_residual=%.17g\n",r.current_step,r.current_time,stage,(int)tree,(int)parent->level,parent->x,parent->y,std::hypot(x.x,x.y),mp,mp-m,mp*u.x-mx,mp*u.y-my,I,Ip,K,Kp,loss,(Ip-I)-loss,Ep-E,V,Vp,Vp/V,p.cell(idDensity_cur),pV/V,pEOS,p.cell(idPressure_cur),pheat,pgeometry,pEOS-pV/V-pheat-pgeometry);
}
}

