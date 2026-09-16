#pragma once
// Included inside RiemannIterationExperiment after Sample and difference.
// Serial frozen-state prototype. Only master-node nonlinear equations are
// solved here; all hanging constraints and caches use the original pipeline.
namespace NonlinearNodes {
struct Stats {int nodes=0,boundary=0,solved=0,multistart=0,failed=0;long long evaluations=0;double assembly_error=0,map_error=0,max_residual=0;};
struct Ref {quad_data_t*d;int corner;};
struct Context {double scale;Stats stats;std::unordered_map<quad_data_t*,size_t> indices;const std::vector<Sample>*inputs;};
inline double magnitude(CDoubleVector a){return std::hypot(a.x,a.y);}
struct Eval {CDoubleMatrix M;CDoubleVector b,F,R;double residual;bool valid;};
struct Equation {
 std::vector<Ref> refs;bool boundary=false;Context*ctx;
 Eval operator()(CDoubleVector u)const{
  ++ctx->stats.evaluations;CDoubleMatrix M(0.,0.,0.,0.);CDoubleVector b(0.,0.);
  for(const auto&r:refs){const auto&v=r.d->m_vara;const auto&p=r.d->m_cndata[r.corner].hdata[CHalf_edge_data::cside::plus];const auto&m=r.d->m_cndata[r.corner].hdata[CHalf_edge_data::cside::minus];
   const auto uc=v.cell_vector(idCentroidVelo_cur);const auto d=u-uc;const double speed=magnitude(d),ap=p.Zcp*p.Rcp*p.Lcp,am=m.Zcp*m.Rcp*m.Lcp;CDoubleMatrix C(0.,0.,0.,0.);
   if(speed>m_eps){C.xx=(ap*std::abs(d^p.Ncp)+am*std::abs(d^m.Ncp))/speed;C.yy=C.xx;}
   else {C.xx=ap*p.Ncp.x*p.Ncp.x+am*m.Ncp.x*m.Ncp.x;C.xy=ap*p.Ncp.x*p.Ncp.y+am*m.Ncp.x*m.Ncp.y;C.yx=C.xy;C.yy=ap*p.Ncp.y*p.Ncp.y+am*m.Ncp.y*m.Ncp.y;}
   M+=C;b+=(p.Lcp*p.Ncp+m.Lcp*m.Ncp)*v.cell(idPressure_cur)+GeometryAlg::MatrixDotVector(C,uc);
  }
  Eval e;e.M=M;e.b=b;e.valid=false;e.residual=INFINITY;
  const double det=M.xx*M.yy-M.xy*M.yx;if(!std::isfinite(det)||det<=0)return e;
  const auto&r=refs.front();
  e.F=boundary?CornerSolve::boundary_node_velocity(r.d->points[r.corner].TwoBouns[0],r.d->points[r.corner].TwoBouns[1],M,b):GeometryAlg::MatrixDotVector(GeometryAlg::MatrixInverse(M),b);
  if(std::abs(e.F.x)<m_eps)e.F.x=0;if(std::abs(e.F.y)<m_eps)e.F.y=0;
  // Interior force residual; constrained boundaries use a scaled fixed-map
  // residual because wall reactions need not vanish in the normal direction.
  e.R=boundary?.5*(M.xx+M.yy)*(u-e.F):GeometryAlg::MatrixDotVector(M,u)-b;
  e.residual=magnitude(u-e.F)/ctx->scale;
  e.valid=std::isfinite(e.residual)&&std::isfinite(e.R.x)&&std::isfinite(e.R.y);return e;
 }
};
inline bool newton(const Equation&eq,CDoubleVector start,CDoubleVector&out,double&best){
 CDoubleVector u=start;const double h=1e-7*eq.ctx->scale;
 for(int k=0;k<80;++k){const auto e=eq(u);if(!e.valid)return false;if(e.residual<best){best=e.residual;out=u;}if(e.residual<=1e-10)return true;
  const auto xp=eq(CDoubleVector(u.x+h,u.y)),xm=eq(CDoubleVector(u.x-h,u.y)),yp=eq(CDoubleVector(u.x,u.y+h)),ym=eq(CDoubleVector(u.x,u.y-h));
  if(!xp.valid||!xm.valid||!yp.valid||!ym.valid)return false;
  const double a=(xp.R.x-xm.R.x)/(2*h),c=(xp.R.y-xm.R.y)/(2*h),b=(yp.R.x-ym.R.x)/(2*h),d=(yp.R.y-ym.R.y)/(2*h),det=a*d-b*c;
  if(!std::isfinite(det)||std::abs(det)<1e-26)return false;
  const CDoubleVector du((-d*e.R.x+b*e.R.y)/det,(c*e.R.x-a*e.R.y)/det);bool accepted=false;
  double alpha=1.;for(int j=0;j<30;++j,alpha*=.5){const auto trial=u+alpha*du;const auto z=eq(trial);if(z.valid&&magnitude(z.R)<magnitude(e.R)*(1-1e-4*alpha)){u=trial;accepted=true;break;}}
  if(!accepted)return false;
 }return false;
}
inline void visit(p4est_iter_corner_info_t*info,void*userdata){
 auto&ctx=*static_cast<Context*>(userdata);Equation eq;eq.ctx=&ctx;
 for(size_t i=0;i<info->sides.elem_count;++i){auto*s=p4est_iter_cside_array_index_int(&info->sides,int(i));SC_CHECK_ABORT(!s->is_ghost,"Serial Newton saw ghost corner");
  Ref r{static_cast<quad_data_t*>(s->quad->p.user_data),HydroCallbacks::convert_which_corner_to_user_define_index(s->corner)};
  SC_CHECK_ABORT(!r.d->points[r.corner].IsHanging,"Master Newton saw hanging node");eq.refs.push_back(r);
  for(int j=0;j<2;++j)eq.boundary=eq.boundary||(r.d->m_cndata[r.corner].hdata[j].enumBYD!=InnerBoundary);
 }
 SC_CHECK_ABORT(!eq.refs.empty(),"Empty nonlinear node");++ctx.stats.nodes;if(eq.boundary)++ctx.stats.boundary;
 const auto&r=eq.refs.front();const auto origin=r.d->m_vara.corner_vector(idcnVelocity_lag,r.corner);const auto input=(*ctx.inputs)[ctx.indices.at(r.d)].node[r.corner];
 for(const auto&q:eq.refs){SC_CHECK_ABORT(magnitude(origin-q.d->m_vara.corner_vector(idcnVelocity_lag,q.corner))<1e-12*ctx.scale,"Master output copies disagree");SC_CHECK_ABORT(magnitude(input-(*ctx.inputs)[ctx.indices.at(q.d)].node[q.corner])<1e-12*ctx.scale,"Master input copies disagree");}
 // Verify independent assembly against the actual last standard iteration.
 const auto check=eq(input);SC_CHECK_ABORT(check.valid,"Invalid reconstructed node");const auto P=r.d->points[r.corner].MatrixP;const auto B=r.d->points[r.corner].RHS;
 const double matrix_scale=std::max(1.,std::max(std::abs(P.xx)+std::abs(P.yy),magnitude(B)));
 const double ae=std::max(std::max(std::abs(check.M.xx-P.xx),std::abs(check.M.yy-P.yy)),std::max(std::max(std::abs(check.M.xy-P.xy),std::abs(check.M.yx-P.yx)),magnitude(check.b-B)))/matrix_scale;
 const double me=magnitude(check.F-origin)/ctx.scale;ctx.stats.assembly_error=std::max(ctx.stats.assembly_error,ae);ctx.stats.map_error=std::max(ctx.stats.map_error,me);
 SC_CHECK_ABORT(ae<1e-11&&me<1e-10,"Nonlinear node reconstruction disagrees with production map");
 double best=INFINITY;CDoubleVector solution=origin;bool ok=newton(eq,origin,solution,best);
 if(!ok){++ctx.stats.multistart;double radius=0.;for(const auto&q:eq.refs)radius=std::max(radius,magnitude(origin-q.d->m_vara.cell_vector(idCentroidVelo_cur)));
  radius=std::max(radius,1e-6*ctx.scale);
  for(int i=-4;i<=4&&!ok;++i)for(int j=-4;j<=4&&!ok;++j)ok=newton(eq,origin+CDoubleVector(i*.5*radius,j*.5*radius),solution,best);
 }
 const auto final=eq(solution);ctx.stats.max_residual=std::max(ctx.stats.max_residual,final.residual);
 if(ok&&final.valid&&final.residual<=1e-10){++ctx.stats.solved;for(const auto&q:eq.refs)q.d->m_vara.corner_vector(idcnVelocity_lag,q.corner)=solution;}
 else {++ctx.stats.failed;const auto&state=static_cast<P4estBridge*>(info->p4est->user_pointer)->data;std::printf("RIEMANN_NEWTON_FAILED step=%d boundary=%d sides=%zu ux=%.17g uy=%.17g residual=%.17g\n",state.current_step,int(eq.boundary),eq.refs.size(),origin.x,origin.y,final.residual);}
}
inline Stats solve(p4est_t*f,const std::vector<quad_data_t*>&a,const std::vector<Sample>&previous,double scale){
 SC_CHECK_ABORT(f->mpisize==1,"Node Newton currently serial only");Context ctx;ctx.scale=scale;ctx.inputs=&previous;
 for(size_t i=0;i<a.size();++i)ctx.indices.emplace(a[i],i);
 p4est_iterate(f,nullptr,&ctx,nullptr,nullptr,visit);return ctx.stats;
}
}
