#pragma once

#include "prox.hpp"
#include "op/ops.hpp"

namespace rl {

template <typename Scalar = Cx>
struct StackProx final : Prox<Scalar>
{
  using Vector = Eigen::Vector<Scalar, Eigen::Dynamic>;
  using Map = Eigen::Map<Vector>;
  using CMap = Eigen::Map<Vector const>;

  StackProx(std::vector<std::shared_ptr<Prox<Scalar>>> p);

  void apply(float const α, CMap const &x, Map &z) const;
  void apply(std::shared_ptr<Ops::Op<Scalar>> const α, CMap const &x, Map &z) const;
private:
  std::vector<std::shared_ptr<Prox<Scalar>>> proxs;
};

}