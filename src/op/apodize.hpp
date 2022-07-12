#pragma once

#include "gridBase.hpp"

namespace rl {

template <int Rank>
struct ApodizeOp final : Operator<Rank, Rank>
{
  using Parent = Operator<Rank, Rank>;
  using Input = typename Parent::Input;
  using InputDims = typename Parent::InputDims;
  using Output = typename Parent::Output;
  using OutputDims = typename Parent::OutputDims;
  static int const ImgRank = 3;

  ApodizeOp(InputDims const &inSize, GridBase<Cx> *gridder)
  {
    std::copy_n(inSize.begin(), Rank, sz_.begin());
    for (Index ii = 0; ii < Rank - ImgRank; ii++) {
      res_[ii] = 1;
      brd_[ii] = sz_[ii];
    }
    for (Index ii = Rank - ImgRank; ii < Rank; ii++) {
      res_[ii] = sz_[ii];
      brd_[ii] = 1;
    }
    apo_ = gridder->apodization(LastN<ImgRank>(sz_)).template cast<Cx>();
  }

  InputDims inputDimensions() const
  {
    return sz_;
  }

  OutputDims outputDimensions() const
  {
    return sz_;
  }

  template <typename T>
  auto A(T const &x) const
  {
    return x / apo_.reshape(res_).broadcast(brd_);
  }

  template <typename T>
  auto Adj(T const &x) const
  {
    return x / apo_.reshape(res_).broadcast(brd_);
  }

private:
  InputDims sz_, res_, brd_;
  Cx3 apo_;
};

} // namespace rl
