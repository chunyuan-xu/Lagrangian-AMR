#pragma once
#include <array>
#include <cmath>
#include <cstring>
#include <vector>
#include <unordered_map>
#include "defines.h"
#include "mesh/ghost_session.h"
#include "hydro/dissipative_weights_experiment.h"

// Isolated frozen-state nonlinear iteration experiment.  No topology change,
// smoothing, analytical shock position, or alteration of conservative data.
namespace RiemannIterationExperiment {
inline int mode(){static int v=std::getenv("AMR_RIEMANN_ITERATION")?std::atoi(std::getenv("AMR_RIEMANN_ITERATION")):0;return v;}
struct Sample {std::array<CDoubleVector,4> node;};
inline std::vector<quad_data_t*> cells(p4est_t*f){
    std::vector<quad_data_t*> a;
    for(p4est_topidx_t t=f->first_local_tree;t<=f->last_local_tree;++t){
        auto*tree=p4est_tree_array_index(f->trees,t);
        for(size_t i=0;i<tree->quadrants.elem_count;++i)
            a.push_back(static_cast<quad_data_t*>(p4est_quadrant_array_index(&tree->quadrants,i)->p.user_data));
    }return a;
}
inline std::vector<Sample> velocities(const std::vector<quad_data_t*>&a){
    std::vector<Sample> out(a.size());for(size_t i=0;i<a.size();++i)for(int c=0;c<4;++c)
        out[i].node[c]=a[i]->m_vara.corner_vector(idcnVelocity_lag,c);return out;
}
inline void relax_inputs(const std::vector<quad_data_t*>&a,const std::vector<Sample>&before,double omega){
    // Only an internal nonlinear iterate, never a conservative-field filter.
    // The next standard phase overwrites point velocities and constrained-node
    // caches; current planar acoustic parent matrices do not depend on their
    // lagged hanging velocity. Every shared corner copy uses the same blend.
    for(size_t i=0;i<a.size();++i)for(int c=0;c<4;++c){
        auto&u=a[i]->m_vara.corner_vector(idcnVelocity_lag,c);
        u.x=before[i].node[c].x+omega*(u.x-before[i].node[c].x);
        u.y=before[i].node[c].y+omega*(u.y-before[i].node[c].y);
    }
}
struct Change {double all=0,master=0,hanging=0,rms=0;};
#include "diagnostics/riemann_node_dump.h"
inline Change difference(p4est_t*f,const std::vector<quad_data_t*>&a,const std::vector<Sample>&before,double scale){
    double local[3]={0,0,0},global[3],sum=0,gsum=0;long long count=0,gcount=0;
    for(size_t i=0;i<a.size();++i)for(int c=0;c<4;++c){
        const auto u=a[i]->m_vara.corner_vector(idcnVelocity_lag,c);
        const double change=std::hypot(u.x-before[i].node[c].x,u.y-before[i].node[c].y)/scale;
        SC_CHECK_ABORT(std::isfinite(change),"Nonfinite Riemann iteration update");
        local[0]=std::max(local[0],change);int group=a[i]->points[c].IsHanging?2:1;
        local[group]=std::max(local[group],change);sum+=change*change;++count;
    }
    SC_CHECK_MPI(sc_MPI_Allreduce(local,global,3,sc_MPI_DOUBLE,sc_MPI_MAX,f->mpicomm));
    SC_CHECK_MPI(sc_MPI_Allreduce(&sum,&gsum,1,sc_MPI_DOUBLE,sc_MPI_SUM,f->mpicomm));
    SC_CHECK_MPI(sc_MPI_Allreduce(&count,&gcount,1,sc_MPI_LONG_LONG_INT,sc_MPI_SUM,f->mpicomm));
    return {global[0],global[1],global[2],std::sqrt(gsum/std::max(1LL,gcount))};
}
#include "diagnostics/riemann_newton_experiment.h"
template<class Iteration> inline void probe_or_apply(p4est_t*f,GhostSession&session,Iteration iteration){
    if(!mode())return;
    SC_CHECK_ABORT(mode()==1||mode()==2,"Invalid Riemann iteration experiment mode");
    SC_CHECK_ABORT(fixed_iter_num==1,"Riemann experiment requires original single iteration");
    const auto&r=static_cast<P4estBridge*>(f->user_pointer)->data;
    const bool targeted=std::getenv("AMR_RIEMANN_TARGET_NODES")&&std::atoi(std::getenv("AMR_RIEMANN_TARGET_NODES"))==1;
    if(targeted){SC_CHECK_ABORT(mode()==1,"Targeted nodes are read-only");
        if(r.current_step!=3977&&r.current_step!=4091&&r.current_step!=4153)return;}
    SC_CHECK_ABORT(f->mpisize==1&&r.solver_type==p4est_data_t::RiemannSolver::Rotated,
        "Current frozen-state Riemann prototype is serial rotated only");
    const auto a=cells(f);const auto first=velocities(a);auto previous=first,older=first;
    std::vector<std::array<double,3>> history;
    std::vector<unsigned char> snapshot(a.size()*sizeof(quad_data_t));
    for(size_t i=0;i<a.size();++i)std::memcpy(snapshot.data()+i*sizeof(quad_data_t),a[i],sizeof(quad_data_t));
    const auto original_stats=DissipativeWeightsExperiment::stats();
    const int original_count=DissipativeWeightsExperiment::count_iteration();
    const int original_trace=g_trace_riemann_iter;
    double local_scale=0,scale=0;
    for(auto*d:a){const auto&v=d->m_vara;const auto u=v.cell_vector(idCentroidVelo_cur);
        local_scale=std::max(local_scale,std::max(std::hypot(u.x,u.y),v.cell(idSoundSpeed)));
        for(int c=0;c<4;++c){const auto w=v.corner_vector(idcnVelocity_lag,c);local_scale=std::max(local_scale,std::hypot(w.x,w.y));}}
    SC_CHECK_MPI(sc_MPI_Allreduce(&local_scale,&scale,1,sc_MPI_DOUBLE,sc_MPI_MAX,f->mpicomm));
    SC_CHECK_ABORT(std::isfinite(scale)&&scale>0,"Invalid Riemann velocity scale");
    Change first_change{},last{};bool converged=false;int total=1;
    // This is a numerical solve tolerance, not a physical AMR threshold.
    const double tolerance=1e-8;
    const int cap=std::getenv("AMR_RIEMANN_ITERATION_CAP")?std::atoi(std::getenv("AMR_RIEMANN_ITERATION_CAP")):32;
    SC_CHECK_ABORT(cap>=2&&cap<=256,"Invalid Riemann iteration diagnostic cap");
    const double omega=std::getenv("AMR_RIEMANN_ITERATION_DAMPING")?std::atof(std::getenv("AMR_RIEMANN_ITERATION_DAMPING")):1.;
    SC_CHECK_ABORT(omega>0&&omega<=1&&std::isfinite(omega),"Invalid nonlinear iterate damping");
    for(int k=1;k<cap;++k){
        g_trace_riemann_iter=k;DissipativeWeightsExperiment::count_iteration()=k;
        DissipativeWeightsExperiment::stats()=DissipativeWeightsExperiment::Stats{};
        iteration();last=difference(f,a,previous,scale);if(k==1)first_change=last;++total;
        const double two_step=k>1&&omega==1.?difference(f,a,older,scale).all:-1.;
        history.push_back({double(total),last.all,two_step});
        if(last.all<=tolerance){converged=true;break;}
        if(k+1==cap)break; // Keep the last input for node-level residual export.
        // Test the undamped fixed-point residual, not omega times that residual.
        if(omega!=1.&&k+1<cap)relax_inputs(a,previous,omega);
        older=previous;previous=velocities(a);
    }
    const bool newton=std::getenv("AMR_RIEMANN_NEWTON_FALLBACK")&&std::atoi(std::getenv("AMR_RIEMANN_NEWTON_FALLBACK"))==1;
    if(newton&&!converged){
        SC_CHECK_ABORT(!targeted,"Newton fallback and old target exporter cannot be combined");
        const auto ns=NonlinearNodes::solve(f,a,previous,scale);
        // Reuse the full existing pipeline so master, hanging, parent-edge and
        // relaxed-force caches all correspond to the converged velocities.
        double check=-1.;
        for(int j=0;j<3;++j){previous=velocities(a);
            g_trace_riemann_iter=total;DissipativeWeightsExperiment::count_iteration()=total;
            DissipativeWeightsExperiment::stats()=DissipativeWeightsExperiment::Stats{};
            iteration();++total;last=difference(f,a,previous,scale);check=last.all;
        }
        converged=ns.failed==0&&check<=tolerance;
        std::printf("RIEMANN_NEWTON step=%d nodes=%d boundary=%d solved=%d multistart=%d failed=%d evaluations=%lld assembly_error=%.17g map_error=%.17g node_residual=%.17g final_full_residual=%.17g\n",
            r.current_step,ns.nodes,ns.boundary,ns.solved,ns.multistart,ns.failed,ns.evaluations,ns.assembly_error,ns.map_error,ns.max_residual,check);
    }
    const auto shift=difference(f,a,first,scale);
    if(targeted)dump_node(f,a,previous,total,scale);
    // The frozen nonlinear map must not advance the conservative solution.
    for(size_t i=0;i<a.size();++i){
        const auto*saved=snapshot.data()+i*sizeof(quad_data_t);
        const auto*base=reinterpret_cast<const unsigned char*>(a[i]);
        const size_t scalar_offset=reinterpret_cast<const unsigned char*>(a[i]->m_vara.DouCData)-base;
        const size_t vector_offset=reinterpret_cast<const unsigned char*>(a[i]->m_vara.VecCData)-base;
        SC_CHECK_ABORT(std::memcmp(a[i]->m_vara.DouCData,saved+scalar_offset,sizeof(a[i]->m_vara.DouCData))==0&&
            std::memcmp(a[i]->m_vara.VecCData,saved+vector_offset,sizeof(a[i]->m_vara.VecCData))==0,
            "Riemann probe changed frozen cell fields");
    }
    if(mode()==1){
        for(size_t i=0;i<a.size();++i){std::memcpy(a[i],snapshot.data()+i*sizeof(quad_data_t),sizeof(quad_data_t));
            SC_CHECK_ABORT(std::memcmp(a[i],snapshot.data()+i*sizeof(quad_data_t),sizeof(quad_data_t))==0,"Riemann audit restore mismatch");}
        DissipativeWeightsExperiment::stats()=original_stats;g_trace_riemann_iter=original_trace;session.exchange();
    }
    DissipativeWeightsExperiment::count_iteration()=original_count;
    std::printf("RIEMANN_ITERATION step=%d t=%.17g mode=%d n=%zu iterations=%d converged=%d first=%.17g first_master=%.17g first_hanging=%.17g last=%.17g last_master=%.17g last_hanging=%.17g shift=%.17g rms_shift=%.17g scale=%.17g\n",
        r.current_step,r.current_time,mode(),a.size(),total,int(converged),first_change.all,first_change.master,first_change.hanging,last.all,last.master,last.hanging,shift.all,shift.rms,scale);
    if(total>=32)for(const auto&h:history)std::printf("RIEMANN_SEQUENCE step=%d iter=%.0f update=%.17g two_step=%.17g cap=%d\n",r.current_step,h[0],h[1],h[2],cap);
    if(mode()==2)SC_CHECK_ABORT(converged,"Riemann nonlinear solve reached iteration cap");
}
}
