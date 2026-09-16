#pragma once
#include <cstdlib>
#include <limits>
#include <vector>
#include <p4est_iterate.h>
#include "mesh/ghost_session.h"
#include "amr/least_squares_gradient.h"
#include "amr/pressure_experiment.h"
#include "amr/compression_experiment.h"
#include "amr/pressure_sided_experiment.h"
#include "amr/pressure_pair_math.h"
#include "amr/linear_service_sensor.h"

// AMR indicator only: does not change the hydrodynamic reconstruction.
namespace DensityGradientEstimator {
namespace WenoShockSensor {
inline bool enabled() { const char *s=std::getenv("AMR_WENO_SENSOR"); return s && std::atoi(s)!=0; }
inline double threshold_refine() { static const double v=std::getenv("AMR_WENO_REFINE") ? std::atof(std::getenv("AMR_WENO_REFINE")) : .20; return v; }
inline double threshold_coarsen() { static const double v=std::getenv("AMR_WENO_COARSEN") ? std::atof(std::getenv("AMR_WENO_COARSEN")) : .19; return v; }
}
struct Scratch {
    DensityLeastSquares::Fit fit;
    LinearServiceSensor::Fit service_fit;
    double edge_sum[4] = {};
    int edge_count[4] = {};
    double pressure_jump = 0.;
    double pressure_sided = 0.;
    PressurePairExperiment::Flags pair;
};
struct Context {
    GhostSession *session;
    std::vector<Scratch> *cells;
};
struct Leaf {
    const CVariable *value;
    Scratch *scratch;
    int face;
};
inline Leaf leaf(p4est_t *forest, Context &ctx,
                 p4est_iter_face_side_t *side, int child) {
    const bool hanging = side->is_hanging;
    const bool ghost = hanging ? side->is.hanging.is_ghost[child] : side->is.full.is_ghost;
    const p4est_locidx_t id = hanging ? side->is.hanging.quadid[child] : side->is.full.quadid;
    p4est_quadrant_t *quad = hanging ? side->is.hanging.quad[child] : side->is.full.quad;
    if (ghost) {
        if (!quad || !ctx.session->valid_remote_id(id)) return {nullptr, nullptr, side->face};
        return {&ctx.session->remote(id).m_vara, nullptr, side->face};
    }
    SC_CHECK_ABORT(quad && id >= 0, "Invalid local density gradient neighbor");
    p4est_tree_t *tree = p4est_tree_array_index(forest->trees, side->treeid);
    const p4est_locidx_t index = tree->quadrants_offset + id;
    SC_CHECK_ABORT(index >= 0 && (size_t)index < ctx.cells->size(), "Invalid density gradient index");
    return {&static_cast<quad_data_t *>(quad->p.user_data)->m_vara,
            &(*ctx.cells)[index], side->face};
}
inline void observe(const Leaf &target, const Leaf &neighbor) {
    if (!target.scratch) return;
    if (!neighbor.value) {
        target.scratch->fit.invalidate();
        target.scratch->service_fit.density.invalidate();
        target.scratch->service_fit.pressure.invalidate();
        return;
    }
    const auto &a = target.value->cell_vector(idCentroidCoord_cur);
    const auto &b = neighbor.value->cell_vector(idCentroidCoord_cur);
    const double dx = b.x-a.x, dy = b.y-a.y;
    const double drho = neighbor.value->cell(idDensity_cur)-target.value->cell(idDensity_cur);
    const double pa=target.value->cell(idPressure_cur), pb=neighbor.value->cell(idPressure_cur);
    const double weight=PressureExperiment::face_weight() ? std::max(std::abs(pa),std::abs(pb))/std::max(PressureExperiment::maximum_pressure(),1e-12) : PressureExperiment::cell_weight() ? std::abs(pa)/std::max(PressureExperiment::maximum_pressure(),1e-12) : 1.;
    const double jump=weight*std::abs(pb-pa)/(std::abs(pa)+std::abs(pb)+1e-12+PressureExperiment::floor_factor()*PressureExperiment::maximum_pressure());
    target.scratch->pressure_jump=std::max(target.scratch->pressure_jump,
        std::isfinite(jump) ? jump : std::numeric_limits<double>::infinity());
    if(PressureSidedExperiment::mode()>0)
        target.scratch->pressure_sided=std::max(target.scratch->pressure_sided,
            PressureSidedExperiment::high_side(pa,pb,jump));
    target.scratch->fit.add(dx, dy, drho);
    if (WenoShockSensor::enabled()) {
        target.scratch->service_fit.add(dx,dy,target.value->cell(idDensity_cur),
            neighbor.value->cell(idDensity_cur),pa,pb,LinearServiceSensor::mode()==6 ?
                .5*PressureExperiment::floor_factor()*PressureExperiment::maximum_pressure() : 0.);
    }
    const double distance = std::hypot(dx, dy);
    const double raw = distance > 0. ? std::abs(drho)/distance : std::numeric_limits<double>::infinity();
    target.scratch->edge_sum[target.face] += std::isfinite(raw) ? raw : std::numeric_limits<double>::infinity();
    ++target.scratch->edge_count[target.face];
}
inline void face(p4est_iter_face_info_t *info, void *user) {
    // Physical boundaries use only interior neighbors (one-sided fit).
    if (info->sides.elem_count != 2) return;
    auto &ctx = *static_cast<Context *>(user);
    auto *a = p4est_iter_fside_array_index_int(&info->sides, 0);
    auto *b = p4est_iter_fside_array_index_int(&info->sides, 1);
    SC_CHECK_ABORT(!(a->is_hanging && b->is_hanging), "Unexpected double hanging face");
    for (int i=0; i<(a->is_hanging ? 2 : 1); ++i)
        for (int j=0; j<(b->is_hanging ? 2 : 1); ++j) {
            const Leaf left = leaf(info->p4est, ctx, a, i);
            const Leaf right = leaf(info->p4est, ctx, b, j);
            observe(left, right); observe(right, left);
        }
}
inline double pair_eta(const CVariable &v) {
    return PressurePairExperiment::eta(v.cell(idVolume),v.cell(idDensity_cur),v.cell(idCDensityGradient));
}
inline void pair_observe(const Leaf &target,const Leaf &neighbor,const p4est_data_t &run) {
    if(!target.scratch)return;
    const auto flags=neighbor.value ? PressurePairExperiment::face(
        target.value->cell(idPressure_cur),neighbor.value->cell(idPressure_cur),
        pair_eta(*target.value),pair_eta(*neighbor.value),
        PressureExperiment::floor_factor()*PressureExperiment::maximum_pressure(),
        run.refine_err,run.coarsen_error,PressureExperiment::refine(),PressureExperiment::coarsen()) :
        PressurePairExperiment::invalid();
    PressurePairExperiment::merge(target.scratch->pair,flags);
}
inline void pair_face(p4est_iter_face_info_t *info,void *user) {
    if(info->sides.elem_count!=2)return;
    auto &ctx=*static_cast<Context *>(user);
    const auto &run=static_cast<P4estBridge *>(info->p4est->user_pointer)->data;
    auto *a=p4est_iter_fside_array_index_int(&info->sides,0);
    auto *b=p4est_iter_fside_array_index_int(&info->sides,1);
    SC_CHECK_ABORT(!(a->is_hanging&&b->is_hanging),"Unexpected double hanging pressure-pair face");
    for(int i=0;i<(a->is_hanging?2:1);++i)
        for(int j=0;j<(b->is_hanging?2:1);++j) {
            const auto x=leaf(info->p4est,ctx,a,i),y=leaf(info->p4est,ctx,b,j);
            pair_observe(x,y,run);pair_observe(y,x,run);
        }
}
// One synchronous face-neighbor dilation of the pressure sensor only.
// Read the immutable field; write scratch so iteration order cannot cascade.
inline void halo_observe(const Leaf &a, const Leaf &b) {
    if(!a.scratch)return;
    const double p=b.value ? b.value->cell(idAMRPressureJump) : std::numeric_limits<double>::infinity();
    a.scratch->pressure_jump=std::max(a.scratch->pressure_jump,
        std::isfinite(p) ? PressureExperiment::halo_weight()*p : std::numeric_limits<double>::infinity());
}
inline void halo_face(p4est_iter_face_info_t *info, void *user) {
    if(info->sides.elem_count!=2)return;
    auto &ctx=*static_cast<Context *>(user);
    auto *a=p4est_iter_fside_array_index_int(&info->sides,0);
    auto *b=p4est_iter_fside_array_index_int(&info->sides,1);
    SC_CHECK_ABORT(!(a->is_hanging && b->is_hanging), "Unexpected double hanging halo face");
    for(int i=0;i<(a->is_hanging?2:1);++i)
        for(int j=0;j<(b->is_hanging?2:1);++j) {
            const Leaf x=leaf(info->p4est,ctx,a,i),y=leaf(info->p4est,ctx,b,j);
            halo_observe(x,y);halo_observe(y,x);
        }
}
// Returns the local invalid-fit count; optional vectors are for verification.
inline size_t estimate(p4est_t *forest, GhostSession &session,
                       std::vector<DensityLeastSquares::Result> *results = nullptr) {
    session.exchange(); // Current density and physical centroids, including MPI neighbors.
    if (WenoShockSensor::enabled()) {
        SC_CHECK_ABORT(LinearServiceSensor::mode()>=4 && LinearServiceSensor::mode()<=6,
            "Linear service sensor supports modes 4, 5, 6 only; use archived binaries for earlier prototypes");
        const double r=WenoShockSensor::threshold_refine(), c=WenoShockSensor::threshold_coarsen();
        SC_CHECK_ABORT(std::isfinite(r) && std::isfinite(c) && c>=0. && c<r,
            "Linear service thresholds must be finite and 0 <= coarsen < refine");
    }
    SC_CHECK_ABORT(PressurePairExperiment::mode()>=0&&PressurePairExperiment::mode()<=3,
        "Invalid pressure-pair experiment mode");
    if(PressurePairExperiment::mode()>0)
        SC_CHECK_ABORT(PressureSidedExperiment::mode()==0 && !PressureExperiment::halo() &&
            !PressureExperiment::face_weight() && !PressureExperiment::cell_weight() && PressureExperiment::mode()==1,
            "Pressure-pair study requires plain pressure AND, without sided/weighted/halo alternatives");
    SC_CHECK_ABORT(PressureSidedExperiment::mode()>=0 && PressureSidedExperiment::mode()<=3,
        "Invalid pressure-sided experiment mode");
    if(PressureSidedExperiment::mode()>0)
        SC_CHECK_ABORT(!PressureExperiment::halo() && !PressureExperiment::face_weight() &&
            !PressureExperiment::cell_weight() && PressureExperiment::mode()==1,
            "Pressure-sided study requires plain pressure AND, without weighting/halo");
    CompressionExperiment::update(forest);
    if(PressureExperiment::floor_factor()>0. || PressureExperiment::cell_weight() || PressureExperiment::face_weight()) {
        double local=0., global=0.;
        for(p4est_topidx_t t=forest->first_local_tree;t<=forest->last_local_tree;++t) {
            auto *tree=p4est_tree_array_index(forest->trees,t);
            for(size_t q=0;q<tree->quadrants.elem_count;++q) {
                auto *quad=p4est_quadrant_array_index(&tree->quadrants,q);
                local=std::max(local,std::abs(static_cast<quad_data_t *>(quad->p.user_data)->m_vara.cell(idPressure_cur)));
            }
        }
        sc_MPI_Allreduce(&local,&global,1,sc_MPI_DOUBLE,sc_MPI_MAX,forest->mpicomm);
        PressureExperiment::maximum_pressure()=global;
    }
    std::vector<Scratch> cells(forest->local_num_quadrants);
    Context ctx{&session, &cells};
    p4est_iterate(forest, session.get(), &ctx, nullptr, face, nullptr);
    if (results) results->resize(cells.size());
    size_t invalid = 0;
    size_t sided_changed=0,symmetric_refine=0,sided_refine=0,symmetric_retain=0,sided_retain=0;
    for (p4est_topidx_t t=forest->first_local_tree; t<=forest->last_local_tree; ++t) {
        auto *tree = p4est_tree_array_index(forest->trees, t);
        for (size_t q=0; q<tree->quadrants.elem_count; ++q) {
            auto *quad = p4est_quadrant_array_index(&tree->quadrants, q);
            auto &v = static_cast<quad_data_t *>(quad->p.user_data)->m_vara;
            const size_t index = tree->quadrants_offset+q;
            const auto &scratch = cells[index];
            const auto result = scratch.fit.solve();
            v.cell(idCDensityGradient) = result.magnitude;
            v.cell_vector(idAMRDensityGradient) = CDoubleVector(result.x,result.y);
            v.cell(idAMRPressureJump) = scratch.pressure_jump;
            v.cell(idAMRPressureSidedJump) = scratch.pressure_sided;
            v.cell(idAMRWenoSensor) = WenoShockSensor::enabled() ?
                scratch.service_fit.indicator(v.cell(idVolume),LinearServiceSensor::mode()) : 0.;
            if(PressureSidedExperiment::mode()>0) {
                sided_changed+=scratch.pressure_sided<scratch.pressure_jump;
                const auto &run=static_cast<P4estBridge *>(forest->user_pointer)->data;
                const double eta=std::sqrt(v.cell(idVolume))*result.magnitude/
                    std::max(std::abs(v.cell(idDensity_cur)),static_cast<double>(m_eps));
                symmetric_refine+=eta>run.refine_err && scratch.pressure_jump>PressureExperiment::refine();
                sided_refine+=eta>run.refine_err && scratch.pressure_sided>PressureExperiment::refine();
                symmetric_retain+=eta>=run.coarsen_error && scratch.pressure_jump>PressureExperiment::coarsen();
                sided_retain+=eta>=run.coarsen_error && scratch.pressure_sided>PressureExperiment::coarsen();
                if(PressureSidedExperiment::mode()==2) v.cell(idAMRPressureJump)=scratch.pressure_sided;
            }
            invalid += !result.valid;
            if (results) (*results)[index] = result;
            for (int f=0; f<4; ++f) {
                v.edge(idERhoGradient,f) = scratch.edge_count[f] ? scratch.edge_sum[f]/scratch.edge_count[f] : 0.;
                v.corner(idCNRhoGradient,f) = 0.;
            }
        }
    }
    if(PressurePairExperiment::mode()>0) {
        // All local gradients must be complete before exchanging.  The second
        // face pass reads immutable current fields and writes scratch only.
        session.exchange();
        p4est_iterate(forest,session.get(),&ctx,nullptr,pair_face,nullptr);
        size_t ref=0,keep=0,bad=0;
        for(p4est_topidx_t t=forest->first_local_tree;t<=forest->last_local_tree;++t) {
            auto *tree=p4est_tree_array_index(forest->trees,t);
            for(size_t q=0;q<tree->quadrants.elem_count;++q) {
                auto &v=static_cast<quad_data_t *>(p4est_quadrant_array_index(&tree->quadrants,q)->p.user_data)->m_vara;
                const auto flags=cells[tree->quadrants_offset+q].pair;
                v.cell(idAMRPressurePairRefine)=flags.refine;
                v.cell(idAMRPressurePairRetain)=flags.retain;
                ref+=flags.refine>0.;keep+=flags.retain>0.;bad+=!std::isfinite(flags.refine)||!std::isfinite(flags.retain);
            }
        }
        const auto &run=static_cast<P4estBridge *>(forest->user_pointer)->data;
        std::printf("PRESSURE_PAIR rank=%d step=%d t=%.17g mode=%d cells=%zu refine=%zu retain=%zu invalid=%zu\n",
            forest->mpirank,run.current_step,run.current_time,PressurePairExperiment::mode(),cells.size(),ref,keep,bad);
    }
    if(PressureSidedExperiment::mode()>0) {
        const auto &run=static_cast<P4estBridge *>(forest->user_pointer)->data;
        std::printf("PRESSURE_SIDED rank=%d step=%d t=%.17g mode=%d cells=%zu changed=%zu symmetric_refine=%zu sided_refine=%zu symmetric_retain=%zu sided_retain=%zu\n",
            forest->mpirank,run.current_step,run.current_time,PressureSidedExperiment::mode(),cells.size(),sided_changed,
            symmetric_refine,sided_refine,symmetric_retain,sided_retain);
    }
    if(PressureExperiment::halo()) {
        session.exchange(); // Exchange the freshly computed, undilated sensor.
        p4est_iterate(forest,session.get(),&ctx,nullptr,halo_face,nullptr);
        for(p4est_topidx_t t=forest->first_local_tree;t<=forest->last_local_tree;++t) {
            auto *tree=p4est_tree_array_index(forest->trees,t);
            for(size_t q=0;q<tree->quadrants.elem_count;++q)
                static_cast<quad_data_t *>(p4est_quadrant_array_index(&tree->quadrants,q)->p.user_data)->m_vara.cell(idAMRPressureJump)=cells[tree->quadrants_offset+q].pressure_jump;
        }
    }
    return invalid;
}
} // namespace DensityGradientEstimator
