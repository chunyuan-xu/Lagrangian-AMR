#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <p4est.h>
#include "defines.h"
#include "variable.h"
#include "physics/eos.h"
namespace DerivedStateExperiment {
inline int mode(){const char*s=std::getenv("AMR_DERIVED_SYNC");return s?std::atoi(s):0;}
inline bool audit(){const char*s=std::getenv("AMR_DERIVED_AUDIT");return s&&std::atoi(s);}
template<class F> inline void each(p4est_t*f,F op){for(p4est_topidx_t t=f->first_local_tree;t<=f->last_local_tree;++t){auto*tr=p4est_tree_array_index(f->trees,t);for(size_t k=0;k<tr->quadrants.elem_count;++k){auto*q=p4est_quadrant_array_index(&tr->quadrants,k);op(static_cast<quad_data_t*>(q->p.user_data)->m_vara);}}}
inline void record(p4est_t*f,const char*phase){
 if(!audit())return;
 double mx[4]={0,0,0,0},gm[4];int counts[3]={0,0,0},gc[3];
 each(f,[&](CVariable&v){double p=PhysicalAlg::EquationOfState(v.cell(idGamma),v.cell(idDensity_cur),v.cell(idInternalEnergy_cur));double pl=PhysicalAlg::EquationOfState(v.cell(idGamma),v.cell(idDensity_lag),v.cell(idInternalEnergy_lag));double c=PhysicalAlg::CalculateSoundSpeed(v.cell(idGamma),pl,v.cell(idDensity_lag));auto u=v.cell_vector(idCentroidVelo_cur);double dp=std::abs(p-v.cell(idPressure_cur)),dl=std::abs(pl-v.cell(idPressure_lag)),dc=std::abs(c-v.cell(idSoundSpeed));mx[0]=std::max(mx[0],dp);mx[1]=std::max(mx[1],dl);mx[2]=std::max(mx[2],dc);mx[3]=std::max(mx[3],std::abs(v.cell(idTotalEnergy_cur)-v.cell(idInternalEnergy_cur)-.5*(u.x*u.x+u.y*u.y)));counts[0]+=dp>1e-10*std::max(1.,std::abs(p));counts[1]+=dl>1e-10*std::max(1.,std::abs(pl));counts[2]+=dc>1e-10*std::max(1.,std::abs(c));});
 SC_CHECK_MPI(sc_MPI_Allreduce(mx,gm,4,sc_MPI_DOUBLE,sc_MPI_MAX,f->mpicomm));SC_CHECK_MPI(sc_MPI_Allreduce(counts,gc,3,sc_MPI_INT,sc_MPI_SUM,f->mpicomm));
 const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;
 P4EST_GLOBAL_PRODUCTIONF("DERIVED phase=%s step=%d t=%.17g mode=%d badp=%d badplag=%d badc=%d dp=%.17g dplag=%.17g dc=%.17g dekin=%.17g\n",phase,r.current_step,r.current_time,mode(),gc[0],gc[1],gc[2],gm[0],gm[1],gm[2],gm[3]);
}
// Diagnostic experiment: synchronize only EOS-derived fields. Never modify
// mass, geometry, density, internal/total energy, velocities, or AMR tags.
inline void synchronize(p4est_t*f){if(!mode())return;SC_CHECK_ABORT(mode()==1,"Unsupported derived sync mode");each(f,[](CVariable&v){
 v.cell(idPressure_cur)=PhysicalAlg::EquationOfState(v.cell(idGamma),v.cell(idDensity_cur),v.cell(idInternalEnergy_cur));
 v.cell(idPressure_lag)=PhysicalAlg::EquationOfState(v.cell(idGamma),v.cell(idDensity_lag),v.cell(idInternalEnergy_lag));
 v.cell(idPressure_half)=PhysicalAlg::EquationOfState(v.cell(idGamma),v.cell(idDensity_half),v.cell(idInternalEnergy_half));
 v.cell(idSoundSpeed)=PhysicalAlg::CalculateSoundSpeed(v.cell(idGamma),v.cell(idPressure_lag),v.cell(idDensity_lag));
 });}
}
