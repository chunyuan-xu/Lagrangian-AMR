#pragma once
#include <cmath>
#include <cstdlib>
#include <limits>
#include <cstring>
#include "defines.h"
#include "variable.h"

// Read-only experiment diagnostic. Analytic radius NEVER enters AMR decisions.
// Count polygon-wholly-far cells, using exact edge projection, after every
// accepted step AND after the AMR/geometry refresh (before hydro).
namespace NohLocalizationAudit {
inline void record(p4est_t *forest, const char *phase) {
    if (!std::getenv("AMR_LOCALIZATION_AUDIT")) return;
    const auto &run=static_cast<P4estBridge *>(forest->user_pointer)->data;
    if (run.which_case!=ProblemNo::NohCartesian) return;
    const double rs=run.current_time/3.;
    const int order[4]={quad_data_t::LEFTBOTTOM,quad_data_t::RIGHTBOTTOM,quad_data_t::RIGHTUP,quad_data_t::LEFTUP};
    int local[12]={}, global[12]={};
    double max_gap=0., global_gap=0.,max_center_gap=0.,global_center_gap=0.;
    // Read-only late-time tracing of L6 families found by FRONT-DEPENDENCY.
    // Logical coordinates and analytic front appear ONLY in this diagnostic.
    const bool timing=std::getenv("AMR_FRONT_TIMING_AUDIT") && run.current_time>=.59;
    const long long families[4][2]={{452984832,721420288},{721420288,452984832},{218103808,822083584},{822083584,218103808}};
    const long long family_h=P4EST_QUADRANT_LEN(6);
    struct Timing {int n=0,l6=0,l7=0,coarse=0;double units=0.,lo=1e30,hi=0.,pmin=1e30,pmax=0.;};
    Timing stamps[4];
    if(timing)SC_CHECK_ABORT(forest->mpisize==1 && forest->connectivity->num_trees==1,"Family timing audit requires serial single-tree Noh");
    for (p4est_topidx_t t=forest->first_local_tree;t<=forest->last_local_tree;++t) {
        auto *tree=p4est_tree_array_index(forest->trees,t);
        for(size_t q=0;q<tree->quadrants.elem_count;++q) {
            const auto *quad=p4est_quadrant_array_index(&tree->quadrants,q);
            const auto &v=static_cast<const quad_data_t *>(quad->p.user_data)->m_vara;
            double rmin=std::numeric_limits<double>::infinity(), rmax=0.;
            bool inside=true;
            double area2=0.,cxsum=0.,cysum=0.;
            for(int k=0;k<4;++k) {
                const auto a=v.corner_vector(idcnCoords_cur,order[k]);
                const auto b=v.corner_vector(idcnCoords_cur,order[(k+1)%4]);
                const double dx=b.x-a.x,dy=b.y-a.y,den=dx*dx+dy*dy;
                const double cross=a.x*b.y-a.y*b.x;
                area2+=cross;cxsum+=(a.x+b.x)*cross;cysum+=(a.y+b.y)*cross;
                SC_CHECK_ABORT(std::isfinite(den)&&den>0.,"Invalid audit edge");
                const double s=std::max(0.,std::min(1.,-(a.x*dx+a.y*dy)/den));
                rmin=std::min(rmin,std::hypot(a.x+s*dx,a.y+s*dy));
                rmax=std::max(rmax,std::hypot(a.x,a.y));
                inside=inside&&(a.x*b.y-a.y*b.x>=-1e-15);
            }
            if(inside) rmin=0.;
            if(timing)for(int g=0;g<4;++g){
                auto&s=stamps[g];const long long x=families[g][0],y=families[g][1];
                if(quad->level>=6 && quad->x>=x && quad->x<x+family_h && quad->y>=y && quad->y<y+family_h){
                    ++s.n;s.l6+=quad->level==6;s.l7+=quad->level==7;
                    const double h=P4EST_QUADRANT_LEN(quad->level)/static_cast<double>(family_h);s.units+=h*h;
                    s.lo=std::min(s.lo,rmin);s.hi=std::max(s.hi,rmax);s.pmin=std::min(s.pmin,v.cell(idPressure_cur));s.pmax=std::max(s.pmax,v.cell(idPressure_cur));
                } else if(quad->level<6){
                    const long long h=P4EST_QUADRANT_LEN(quad->level);
                    if(x>=quad->x&&x<quad->x+h&&y>=quad->y&&y<quad->y+h)s.coarse=quad->level;
                }
            }
            ++local[0];
            if(quad->level>=4&&quad->level<=7) ++local[1+quad->level-4];
            if(quad->level>=6) {
                SC_CHECK_ABORT(area2>0. && std::isfinite(area2),"Invalid audit signed area");
                const double signed_center_gap=std::hypot(cxsum/(3*area2),cysum/(3*area2))-rs;
                const double center_gap=std::abs(signed_center_gap);
                max_center_gap=std::max(max_center_gap,center_gap);
                if(center_gap>.04) ++local[quad->level==6?10:11];
                if(center_gap>.04 && std::getenv("AMR_ORIGIN_AUDIT"))
                    std::printf("FARCELL phase=%s step=%d t=%.17g tree=%d level=%d x=%lld y=%lld centergap=%.17g polygap=%.17g signedcentergap=%.17g rmin=%.17g rmax=%.17g\n",phase,run.current_step,run.current_time,(int)t,(int)quad->level,(long long)quad->x,(long long)quad->y,center_gap,std::max(rmin-rs,rs-rmax),signed_center_gap,rmin,rmax);
                const double gap=std::max(rmin-rs,rs-rmax);
                max_gap=std::max(max_gap,gap);
                if(rmax<rs-.04) ++local[5];
                if(rmin>rs+.04) ++local[6];
                if(gap>.03) ++local[7];
                if(gap>.05) ++local[8];
                if(gap>.06) ++local[9];
            }
        }
    }
    sc_MPI_Allreduce(local,global,12,sc_MPI_INT,sc_MPI_SUM,forest->mpicomm);
    if(timing)for(int g=0;g<4;++g){const auto&s=stamps[g];
        SC_CHECK_ABORT((s.coarse>0&&s.n==0)||(s.coarse==0&&s.units==1.),"Incomplete family timing coverage");
        std::printf("FRONT_TIMING phase=%s step=%d t=%.17g group=%d x=%lld y=%lld leaves=%d L6=%d L7=%d coarse_level=%d rmin=%.17g rmax=%.17g gap=%.17g pmin=%.17g pmax=%.17g intersects=%d\n",
            phase,run.current_step,run.current_time,g,families[g][0],families[g][1],s.n,s.l6,s.l7,s.coarse,s.n?s.lo:-1.,s.n?s.hi:-1.,s.n?s.lo-rs:1e30,s.n?s.pmin:-1.,s.n?s.pmax:-1.,s.n&&s.lo<=rs&&s.hi>=rs);
    }
    sc_MPI_Allreduce(&max_gap,&global_gap,1,sc_MPI_DOUBLE,sc_MPI_MAX,forest->mpicomm);
    sc_MPI_Allreduce(&max_center_gap,&global_center_gap,1,sc_MPI_DOUBLE,sc_MPI_MAX,forest->mpicomm);
    P4EST_GLOBAL_PRODUCTIONF("LOC phase=%s step=%d t=%.17g n=%d L4=%d L5=%d L6=%d L7=%d behind04=%d ahead04=%d far03=%d far05=%d far06=%d maxgap=%.17g centerL6=%d centerL7=%d maxcentergap=%.17g\n",phase,run.current_step,run.current_time,global[0],global[1],global[2],global[3],global[4],global[5],global[6],global[7],global[8],global[9],global_gap,global[10],global[11],global_center_gap);
}
}
