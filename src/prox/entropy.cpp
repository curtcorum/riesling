#include "entropy.hpp"

#include "log.hpp"
#include "tensorOps.hpp"

namespace rl {

Entropy::Entropy(float const λ)
  : Prox<Cx>()
  , λ_{λ}
{
  Log::Print(FMT_STRING("Entropy Prox λ {}"), λ);
}

void Entropy::operator()(float const α, Vector const &v, Vector &z) const
{
  float const t = α * λ_;
  Eigen::ArrayXf const vabs = v.array().abs();
  Eigen::ArrayXf x = vabs;
  for (int ii = 0; ii < 16; ii++) {
    auto const g = (x > 0.f).select((x.log() + 1.f) + (1.f / t) * (x - vabs), 0.f);
    x = (x - (t / 2.f) * g).cwiseMax(0.f);
  }
  z = v.array() * (x / vabs);
  Log::Print(FMT_STRING("Entropy α {} λ {} t {} |v| {} |z| {}"), α, λ_, t, v.norm(), z.norm());
}

NMREntropy::NMREntropy(float const λ)
  : Prox<Cx>()
  , λ_{λ}
{
  Log::Print(FMT_STRING("NMR Entropy Prox λ {}"), λ_);
}

void NMREntropy::operator()(float const α, Vector const &v, Vector &z) const
{
  float const t = α * λ_;
  Eigen::ArrayXf const vabs = v.array().abs();
  Eigen::ArrayXf x = vabs;
  for (int ii = 0; ii < 16; ii++) {
    auto const xx = (x.square() + 1.f).sqrt();
    auto const g = ((x * (x / xx + 1.f)) / (x + xx) + (x + xx).log() - x / xx) + (1.f / t) * (x - vabs);
    x = (x - (t / 2.f) * g).cwiseMax(0.f);
  }
  z = v.array() * (x / vabs);
  Log::Print(FMT_STRING("NMR Entropy α {} λ {} t {} |v| {} |z| {}"), α, λ_, t, v.norm(), z.norm());
}

} // namespace rl