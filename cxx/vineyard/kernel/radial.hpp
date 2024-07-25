#pragma once

#include "fixed.hpp"
#include "tensors.hpp"

namespace rl {

template <typename Scalar, int ND, typename Func> struct Radial final : Kernel<Scalar, ND>
{
  static constexpr int   Width = Func::Width;
  static constexpr int   PadWidth = Func::PadWidth;
  static constexpr float HalfWidth = Width / 2.f;
  using OneD = Eigen::TensorFixedSize<float, Eigen::Sizes<Func::PadWidth>>;
  using Tensor = typename FixedKernel<float, ND, PadWidth>::Tensor;
  using Point = typename FixedKernel<float, ND, PadWidth>::Point;
  using Pos = typename FixedKernel<float, ND, PadWidth>::OneD;

  Func  f;
  float β, scale;
  OneD  centers;

  Radial(float const osamp)
    : β{f.β(osamp)}
    , scale{1.f}
  {
    static_assert(ND < 4);
    for (int ii = 0; ii < Func::PadWidth; ii++) {
      this->centers(ii) = ii + 0.5f - (Func::PadWidth / 2.f);
    }
    scale = 1. / Norm((*this)(Point::Zero()));
    Log::Print("Radial, scale {}", scale);
  }

  virtual auto paddedWidth() const -> int final { return Func::PadWidth; }

  inline auto operator()(Point const p) const -> Eigen::Tensor<float, ND> final { return this->dist2(p); }

  inline auto dist2(Point const p) const -> Tensor
  {
    Tensor    z;
    Pos const z1 = ((this->centers - p(ND - 1)) / HalfWidth).square();
    if constexpr (ND == 1) {
      z = z1;
    } else {
      Pos const z2 = ((this->centers - p(ND - 2)) / HalfWidth).square();
      if constexpr (ND == 2) {
        z = z2.reshape(Sz2{PadWidth, 1}).broadcast(Sz2{1, PadWidth}) + z1.reshape(Sz2{1, PadWidth}).broadcast(Sz2{PadWidth, 1});
      } else {
        Pos const z3 = ((this->centers - p(ND - 3)) / HalfWidth).square();
        z = z3.reshape(Sz3{PadWidth, 1, 1}).broadcast(Sz3{1, PadWidth, PadWidth}) +
            z2.reshape(Sz3{1, PadWidth, 1}).broadcast(Sz3{PadWidth, 1, PadWidth}) +
            z1.reshape(Sz3{1, 1, PadWidth}).broadcast(Sz3{PadWidth, PadWidth, 1});
      }
    }
    return f(z, β) * z.constant(scale);
  }

  void spread(std::array<int16_t, ND> const   c,
              Point const                    &p,
              Eigen::Tensor<Scalar, 1> const &b,
              Eigen::Tensor<Scalar, 1> const &y,
              Eigen::Tensor<Scalar, ND + 2>  &x) const final
  {
    Tensor const d = this->dist2(p);
    FixedKernel<Scalar, ND, Func::PadWidth>::Spread(c, d, b, y, x);
  }

  void gather(std::array<int16_t, ND> const                                c,
              Point const                                                 &p,
              Eigen::Tensor<Scalar, 1> const                              &b,
              Eigen::TensorMap<Eigen::Tensor<Scalar, ND + 2> const> const &x,
              Eigen::TensorMap<Eigen::Tensor<Scalar, 1>>                  &y) const final
  {
    Tensor const d = this->dist2(p);
    FixedKernel<Scalar, ND, Func::PadWidth>::Gather(c, d, b, x, y);
  }
};
} // namespace rl
