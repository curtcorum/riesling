#pragma once

#include "zte.hpp"

namespace rl {

struct DIR final : SegmentedZTE
{
  DIR(Pars const s, bool const pt);

  auto nTissueParameters() const -> Index;
  auto simulate(Eigen::ArrayXf const &p) const -> Cx2;
  auto timepoints() const -> Re1;
};

} // namespace rl
