#pragma once

#include "types.hpp"

#include "io/writer.hpp"

namespace rl {

struct SVDBasis
{
  SVDBasis(Eigen::ArrayXXf const &dynamics,
           Index const            nB,
           bool const             demean,
           bool const             rotate,
           bool const             normalize,
           Index const            segSize = -1);
  Eigen::MatrixXf basis;
};

} // namespace rl