#include "lsqr.hpp"

#include "bidiag.hpp"
#include "common.hpp"
#include "iter.hpp"
#include "log.hpp"

namespace rl {

auto LSQR::run(Vector const &b, float const λ, Vector const &x0) const -> Vector
{
  return run(CMap{b.data(), b.rows()}, λ, CMap{x0.data(), x0.rows()});
}

/* Based on https://github.com/PythonOptimizers/pykrylov/blob/master/pykrylov/lls/lsqr.py
 */
auto LSQR::run(CMap const b, float const λ, CMap const x0) const -> Vector
{
  Log::Print("LSQR", "λ {}", λ);
  if (iterLimit < 1) { throw Log::Failure("LSQR", "Requires at least 1 iteration"); }
  Index const rows = op->rows();
  Index const cols = op->cols();
  if (rows < 1 || cols < 1) { throw Log::Failure("LSQR", "Invalid operator size rows {} cols {}", rows, cols); }
  if (b.rows() != rows) { throw Log::Failure("LSQR", "b had size {}, expected {}", b.rows(), rows); }
  Vector Mu(rows), u(rows);
  Vector x(cols), v(cols), w(cols);

  float α = 0.f, β = 0.f;
  BidiagInit(op, Minv, Mu, u, v, α, β, x, b, x0);
  w = v;

  float ρ̅ = α;
  float ɸ̅ = β;
  float normb = β, xxnorm = 0.f, ddnorm = 0.f, res2 = 0.f, z = 0.f;
  float normA = 0.f;
  float cs2 = -1.f;
  float sn2 = 0.f;

  Log::Print("LSQR", "IT |x|       |r|       |A'r|     |A|       cond(A)");
  Log::Print("LSQR", "{:02d} {:4.3E} {:4.3E} {:4.3E}", 0, ParallelNorm(x), β, std::fabs(α * β));
  Iterating::Starting();
  for (Index ii = 0; ii < iterLimit; ii++) {
    Bidiag(op, Minv, Mu, u, v, α, β);

    float c, s, ρ;
    float ψ = 0.f;
    if (λ == 0.f) {
      std::tie(c, s, ρ) = StableGivens(ρ̅, β);
    } else {
      // Deal with regularization
      auto [c1, s1, ρ̅1] = StableGivens(ρ̅, λ);
      ψ = s1 * ɸ̅;
      ɸ̅ = c1 * ɸ̅;
      std::tie(c, s, ρ) = StableGivens(ρ̅1, β);
    }
    float const ɸ = c * ɸ̅;
    ɸ̅ = s * ɸ̅;
    float const τ = s * ɸ;
    float const θ = s * α;
    ρ̅ = -c * α;
    x = x + w * (ɸ / ρ);
    w = v - w * (θ / ρ);

    // Estimate norms
    float const δ = sn2 * ρ;
    float const ɣ̅ = -cs2 * ρ;
    float const rhs = ɸ - δ * z;
    float const zbar = rhs / ɣ̅;
    float const normx = std::sqrt(xxnorm + zbar * zbar);
    float       ɣ;
    std::tie(cs2, sn2, ɣ) = StableGivens(ɣ̅, θ);
    z = rhs / ɣ;
    xxnorm += z * z;
    ddnorm = ddnorm + std::pow(ParallelNorm(w), 2) / (ρ * ρ);

    normA = std::sqrt(normA * normA + α * α + β * β + λ * λ);
    float const condA = normA * std::sqrt(ddnorm);
    float const res1 = ɸ̅ * ɸ̅;
    res2 = res2 + ψ * ψ;
    float const normr = std::sqrt(res1 + res2);
    float const normAr = α * std::abs(τ);

    Log::Print("LSQR", "{:02d} {:4.3E} {:4.3E} {:4.3E} {:4.3E} {:4.3E}", ii + 1, normx, normr, normAr, normA, condA);
    if (debug) { debug(ii, x); }
    if (1.f + (1.f / condA) <= 1.f) {
      Log::Print("LSQR", "Cond(A) is very large");
      break;
    }
    if ((1.f / condA) <= cTol) {
      Log::Print("LSQR", "Cond(A) has exceeded limit");
      break;
    }

    if (1.f + (normAr / (normA * normr)) <= 1.f) {
      Log::Print("LSQR", "Least-squares solution reached machine precision");
      break;
    }
    if ((normAr / (normA * normr)) <= aTol) {
      Log::Print("LSQR", "Least-squares = {:4.3E} < {:4.3E}", normAr / (normA * normr), aTol);
      break;
    }

    if (normr <= (bTol * normb + aTol * normA * normx)) {
      Log::Print("LSQR", "Ax - b <= aTol, bTol");
      break;
    }
    if ((1.f + normr / (normb + normA * normx)) <= 1.f) {
      Log::Print("LSQR", "Ax - b reached machine precision");
      break;
    }
    if (Iterating::ShouldStop("LSQR")) { break; }
  }
  Iterating::Finished();
  return x;
}

} // namespace rl
