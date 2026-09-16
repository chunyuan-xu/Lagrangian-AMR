#pragma once
#include <cmath>
#include <cstdlib>
#include <limits>
#include "defines.h"
#include "variable.h"
// Isolated startup-resolution experiment. No radius or analytic shock state.
namespace CompressionExperiment {
inline double threshold() { static double x=std::getenv("AMR_COMPRESSION_REFINE")?std::atof(std::getenv("AMR_COMPRESSION_REFINE")):0.;return x; }
inline double initial_threshold() { static double x=std::getenv("AMR_INITIAL_COMPRESSION_REFINE")?std::atof(std::getenv("AMR_INITIAL_COMPRESSION_REFINE")):threshold();return x; }
inline bool enabled() { return threshold()>0.; }
inline bool runtime_enabled() { return enabled() && std::getenv("AMR_COMPRESSION_SEED_ONLY")==nullptr; }
inline double &speed() { static double x=0.;return x; }
inline void update(p4est_t *forest) {
    if(!enabled())return;
    double local=0.,global=0.;
    for(p4est_topidx_t t=forest->first_local_tree;t<=forest->last_local_tree;++t) {
        auto *tree=p4est_tree_array_index(forest->trees,t);
        for(size_t k=0;k<tree->quadrants.elem_count;++k) {
            const auto &v=static_cast<quad_data_t *>(p4est_quadrant_array_index(&tree->quadrants,k)->p.user_data)->m_vara;
            const auto u=v.cell_vector(idCentroidVelo_cur);local=std::max(local,std::hypot(u.x,u.y));
            for(int c=0;c<4;++c) {const auto w=v.corner_vector(idcnVelocity_cur,c);local=std::max(local,std::hypot(w.x,w.y));}
        }
    }
    sc_MPI_Allreduce(&local,&global,1,sc_MPI_DOUBLE,sc_MPI_MAX,forest->mpicomm);speed()=global;
}
inline double indicator(const CVariable &v) {
    if(!enabled())return 0.;
    const int order[4]={quad_data_t::LEFTBOTTOM,quad_data_t::RIGHTBOTTOM,quad_data_t::RIGHTUP,quad_data_t::LEFTUP};double area2=0.,flux=0.;
    for(int c=0;c<4;++c) {
        const auto a=v.corner_vector(idcnCoords_cur,order[c]),b=v.corner_vector(idcnCoords_cur,order[(c+1)%4]);
        const auto u=v.corner_vector(idcnVelocity_cur,order[c]),w=v.corner_vector(idcnVelocity_cur,order[(c+1)%4]);
        area2+=a.x*b.y-a.y*b.x;
        flux+=.5*((u.x+w.x)*(b.y-a.y)-(u.y+w.y)*(b.x-a.x));
    }
    const double area=.5*area2;
    if(!(area>0.)||!std::isfinite(flux))return std::numeric_limits<double>::infinity();
    return std::max(-flux,0.)/(std::sqrt(area)*std::max(speed(),1e-12));
}
inline int initial_refine(p4est_t *forest,p4est_topidx_t,p4est_quadrant_t *q) {
    const auto &run=static_cast<P4estBridge *>(forest->user_pointer)->data;
    return q->level<run.max_level && indicator(static_cast<quad_data_t *>(q->p.user_data)->m_vara)>initial_threshold();
}
}
