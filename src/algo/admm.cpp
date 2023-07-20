#include "admm.hpp"

#include "log.hpp"
#include "lsmr.hpp"
#include "op/tensorop.hpp"
#include "signals.hpp"
#include "tensorOps.hpp"

namespace rl {

namespace {
auto CurveAdaptive(float const sd, float const mg) -> float
{
  if ((mg / sd) > 0.5f) {
    return mg;
  } else {
    return sd - 0.5f * mg;
  }
}

} // namespace

auto ADMM::run(Cx const *bdata, float const ρ) const -> Vector
{
  /* See https://web.stanford.edu/~boyd/papers/admm/lasso/lasso_lsqr.html
   * For the least squares part we are solving:
  A'x = b'
  [     A            [     b
    √ρ F_1             √ρ (z_1 - u_1)
    √ρ F_2     x =     √ρ (z_2 - u_2)
      ...              ...
    √ρ F_n ]           √ρ (z_n - u_n) ]

    Then for the regularizers, in turn we do:

    z_i = prox_λ/ρ(F_i * x + u_i)
    u_i = F_i * x + u_i - z_i
    */

  Index const                                      R = reg_ops.size();
  std::vector<Vector>                              Fx0(R), z(R), z0(R), u(R), u0(R), û0(R);
  std::vector<float>                               ρs(R);
  std::vector<std::shared_ptr<Ops::DiagScale<Cx>>> ρdiags(R);
  std::vector<std::shared_ptr<Ops::Op<Cx>>>        scaled_ops(R);
  for (Index ir = 0; ir < R; ir++) {
    Index const sz = reg_ops[ir]->rows();
    z[ir].resize(sz);
    z[ir].setZero();
    z0[ir].resize(sz);
    z0[ir].setZero();
    u[ir].resize(sz);
    u[ir].setZero();
    u0[ir].resize(sz);
    u0[ir].setZero();
    û0[ir].resize(sz);
    û0[ir].setZero();
    Fx0[ir].resize(sz);
    Fx0[ir].setZero();
    ρs[ir] = ρ;
    ρdiags[ir] = std::make_shared<Ops::DiagScale<Cx>>(sz, std::sqrt(ρs[ir]));
    scaled_ops[ir] = std::make_shared<Ops::Multiply<Cx>>(ρdiags[ir], reg_ops[ir]);
  }

  std::shared_ptr<Op> reg = std::make_shared<Ops::VStack<Cx>>(scaled_ops);
  std::shared_ptr<Op> Aʹ = std::make_shared<Ops::VStack<Cx>>(A, reg);
  std::shared_ptr<Op> I = std::make_shared<Ops::Identity<Cx>>(reg->rows());
  std::shared_ptr<Op> Mʹ = std::make_shared<Ops::DStack<Cx>>(M, I);

  LSMR lsmr{Aʹ, Mʹ, lsqLimit, aTol, bTol, cTol};

  Vector x(A->cols());
  x.setZero();
  CMap const b(bdata, A->rows());

  Vector bʹ(Aʹ->rows());
  bʹ.setZero();
  bʹ.head(A->rows()) = b;

  Log::Print("ADMM ε {:4.3E}", ε);
  PushInterrupt();
  for (Index io = 0; io < outerLimit; io++) {
    Index start = A->rows();
    for (Index ir = 0; ir < R; ir++) {
      Index rr = reg_ops[ir]->rows();
      bʹ.segment(start, rr) = std::sqrt(ρs[ir]) * (z[ir] - u[ir]);
      start += rr;
      ρdiags[ir]->scale = std::sqrt(ρs[ir]);
    }
    x = lsmr.run(bʹ.data(), 0.f, x.data());
    float const normx = x.norm();
    Log::Print("ADMM Iter {:02d} |x| {:4.3E}", io, normx);
    if (debug_x) { debug_x(io, x); }

    bool converged = true;
    for (Index ir = 0; ir < R; ir++) {
      Vector const zprev = z[ir];
      Vector const uprev = u[ir];
      Vector const Fx = reg_ops[ir]->forward(x);
      Vector const Fxpu = Fx + u[ir];
      prox[ir]->apply(1.f / ρs[ir], Fxpu, z[ir]);
      u[ir] = Fxpu - z[ir];
      float const normFx = Fx.norm();
      float const normz = z[ir].norm();
      float const normu = reg_ops[ir]->adjoint(u[ir]).norm();
      if (debug_z) { debug_z(io, ir, Fx, u[ir], z[ir]); }
      // Relative residuals as per Wohlberg 2017
      float const pRes = (Fx - z[ir]).norm() / std::max(normFx, normz);
      float const dRes = reg_ops[ir]->adjoint(z[ir] - zprev).norm() / normu;

      Log::Print("Reg {:02d} ρ {:4.3E} |Fx| {:4.3E} |z| {:4.3E} |u| {:4.3E} |Primal| {:4.3E} |Dual| {:4.3E}", //
                 ir, ρs[ir], normFx, normz, normu, pRes, dRes);
      if ((pRes > ε) || (dRes > ε)) { converged = false; }

      float const τmax = 100.f;
      float const ratio = std::sqrt(pRes / dRes / ε);
      float const τ = (ratio < 1) ? std::max(1.f / τmax, 1.f / ratio) : std::min(τmax, ratio);
      float const μ = 2.f;
      if (io > 0) { // z_0 is zero, so perfectly reasonable dual residuals can trigger this
        if (pRes > μ * dRes) {
          ρs[ir] *= τ;
          u[ir] /= τ;
        } else if (dRes > μ * pRes) {
          ρs[ir] /= τ;
          u[ir] *= τ;
        }
      }

      // if (io == 0) {
      //   u0[ir] = u[ir];
      //   û0[ir] = uprev + Fx - zprev;
      //   z0[ir] = z[ir];
      //   Fx0[ir] = Fx;
      // }
      // if (io % 2 == 1) {
      //   Vector const û = uprev + Fx - zprev;
      //   Vector const ΔFx = Fx - Fx0[ir];
      //   Vector const Δz = z[ir] - z0[ir];
      //   Vector const Δu = u[ir] - u0[ir];
      //   Vector const Δû = û - û0[ir];
      //   float const  normΔFx = ΔFx.norm();
      //   float const  normΔz = Δz.norm();
      //   float const  normΔû = Δû.norm();
      //   float const  normΔu = Δu.norm();
      //   float const  ΔFx_dot_Δû = ΔFx.dot(Δû).real();
      //   float const  Δz_dot_Δu = Δz.dot(Δu).real();

      //   float const α̂sd = (normΔû * normΔû) / ΔFx_dot_Δû;
      //   float const α̂mg = ΔFx_dot_Δû / (normΔFx * normΔFx);
      //   float const α̂ = CurveAdaptive(α̂sd, α̂mg);

      //   float const β̂sd = (normΔu * normΔu) / Δz_dot_Δu;
      //   float const β̂mg = Δz_dot_Δu / (normΔz * normΔz);
      //   float const β̂ = CurveAdaptive(β̂sd, β̂mg);

      //   float const εcor = 0.2f;
      //   bool const  αflag = (ΔFx_dot_Δû > εcor * normΔFx * normΔû) ? true : false;
      //   bool const  βflag = (normΔz > εcor * normΔz * normΔu) ? true : false;

      //   // Update ρ
      //   float const ρold = ρs[ir];
      //   if (αflag && βflag) {
      //     ρs[ir] = std::sqrt(α̂ * β̂);
      //   } else if (αflag) {
      //     ρs[ir] = α̂;
      //   } else if (βflag) {
      //     ρs[ir] = β̂;
      //   }

      //   // Update variables including rescaling anything to do with u
      //   float const τ = ρold / ρs[ir];
      //   u[ir] = u[ir] * τ;
      //   Fx0[ir] = Fx;
      //   z0[ir] = z[ir];
      //   u0[ir] = u[ir];
      //   û0[ir] = uprev * τ + Fx - zprev;
      // }
    }
    if (converged) {
      Log::Print("All primal and dual tolerances achieved, stopping");
      break;
    }
    if (InterruptReceived()) { break; }
  }
  PopInterrupt();
  return x;
}

} // namespace rl
