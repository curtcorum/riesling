#include "../tensorOps.h"
#include "../threads.h"
#include "sense.h"

SenseOp::SenseOp(Output &maps, Output::Dimensions const &bigSize)
    : maps_{std::move(maps)}
{
  auto const smallSize = maps_.dimensions();
  std::copy_n(bigSize.begin(), 4, full_.begin());
  std::copy_n(smallSize.begin(), 4, size_.begin());
  std::transform(
      bigSize.begin(), bigSize.end(), smallSize.begin(), left_.begin(), [](long big, long small) {
        return (big - small + 1) / 2;
      });
  std::transform(
      bigSize.begin(), bigSize.end(), smallSize.begin(), right_.begin(), [](long big, long small) {
        return (big - small) / 2;
      });
}

long SenseOp::channels() const
{
  return maps_.dimension(0);
}

Sz3 SenseOp::dimensions() const
{
  return Sz3{maps_.dimension(1), maps_.dimension(2), maps_.dimension(3)};
}

void SenseOp::A(Input const &x, Output &y) const
{
  assert(x.dimension(0) == maps_.dimension(1));
  assert(x.dimension(1) == maps_.dimension(2));
  assert(x.dimension(2) == maps_.dimension(3));
  assert(y.dimension(0) == maps_.dimension(0));
  assert(y.dimension(1) == (maps_.dimension(1) + left_[1] + right_[1]));
  assert(y.dimension(2) == (maps_.dimension(2) + left_[2] + right_[2]));
  assert(y.dimension(3) == (maps_.dimension(3) + left_[3] + right_[3]));

  Eigen::IndexList<Eigen::type2index<1>, int, int, int> res;
  res.set(1, x.dimension(0));
  res.set(2, x.dimension(1));
  res.set(3, x.dimension(2));
  Eigen::IndexList<int, Eigen::type2index<1>, Eigen::type2index<1>, Eigen::type2index<1>> brd;
  brd.set(0, maps_.dimension(0));

  Eigen::array<std::pair<int, int>, 4> paddings;
  std::transform(
      left_.begin(), left_.end(), right_.begin(), paddings.begin(), [](long left, long right) {
        return std::make_pair(left, right);
      });

  y.device(Threads::GlobalDevice()) = (x.reshape(res).broadcast(brd) * maps_).pad(paddings);
}

void SenseOp::Adj(Output const &x, Input &y) const
{
  assert(x.dimension(0) == maps_.dimension(0));
  assert(x.dimension(1) == (maps_.dimension(1) + left_[1] + right_[1]));
  assert(x.dimension(2) == (maps_.dimension(2) + left_[2] + right_[2]));
  assert(x.dimension(3) == (maps_.dimension(3) + left_[3] + right_[3]));
  assert(y.dimension(0) == maps_.dimension(1));
  assert(y.dimension(1) == maps_.dimension(2));
  assert(y.dimension(2) == maps_.dimension(3));
  y.device(Threads::GlobalDevice()) = ConjugateSum(x.slice(left_, size_), maps_);
}

void SenseOp::AdjA(Input const &x, Input &y) const
{
  y.device(Threads::GlobalDevice()) = x;
}

SenseBasisOp::SenseBasisOp(Cx4 &maps, Output::Dimensions const &bigSize)
    : maps_{std::move(maps)}
{
  size_[0] = maps_.dimension(0);
  size_[1] = bigSize[1];
  size_[2] = maps_.dimension(1);
  size_[3] = maps_.dimension(2);
  size_[4] = maps_.dimension(3);
  full_[0] = maps_.dimension(0);
  full_[1] = bigSize[1];
  full_[2] = bigSize[2];
  full_[3] = bigSize[3];
  full_[4] = bigSize[4];
  left_[0] = right_[0] = 0;
  left_[1] = right_[1] = 0;
  left_[2] = (full_[2] - size_[2] + 1) / 2;
  left_[3] = (full_[3] - size_[3] + 1) / 2;
  left_[4] = (full_[4] - size_[4] + 1) / 2;
  right_[2] = (full_[2] - size_[2]) / 2;
  right_[3] = (full_[3] - size_[3]) / 2;
  right_[4] = (full_[4] - size_[4]) / 2;
}

long SenseBasisOp::channels() const
{
  return maps_.dimension(0);
}

Sz3 SenseBasisOp::dimensions() const
{
  return Sz3{maps_.dimension(1), maps_.dimension(2), maps_.dimension(3)};
}

void SenseBasisOp::A(Input const &x, Output &y) const
{
  assert(x.dimension(1) == maps_.dimension(1));
  assert(x.dimension(2) == maps_.dimension(2));
  assert(x.dimension(3) == maps_.dimension(3));
  assert(y.dimension(0) == maps_.dimension(0));
  assert(y.dimension(1) == x.dimension(0));
  assert(y.dimension(2) == (maps_.dimension(1) + left_[2] + right_[2]));
  assert(y.dimension(3) == (maps_.dimension(2) + left_[3] + right_[3]));
  assert(y.dimension(4) == (maps_.dimension(3) + left_[4] + right_[4]));

  Eigen::IndexList<Eigen::type2index<1>, int, int, int, int> res;
  res.set(1, x.dimension(0));
  res.set(2, x.dimension(1));
  res.set(3, x.dimension(2));
  res.set(4, x.dimension(3));
  Eigen::IndexList<
      int,
      Eigen::type2index<1>,
      Eigen::type2index<1>,
      Eigen::type2index<1>,
      Eigen::type2index<1>>
      brd;
  brd.set(0, maps_.dimension(0));

  Eigen::array<std::pair<int, int>, 5> paddings;
  std::transform(
      left_.begin(), left_.end(), right_.begin(), paddings.begin(), [](long left, long right) {
        return std::make_pair(left, right);
      });

  y.device(Threads::GlobalDevice()) = (x.reshape(res).broadcast(brd) * maps_).pad(paddings);
}

void SenseBasisOp::Adj(Output const &x, Input &y) const
{
  assert(x.dimension(0) == maps_.dimension(0));
  assert(x.dimension(1) == (maps_.dimension(1) + left_[1] + right_[1]));
  assert(x.dimension(2) == (maps_.dimension(2) + left_[2] + right_[2]));
  assert(x.dimension(3) == (maps_.dimension(3) + left_[3] + right_[3]));
  assert(y.dimension(0) == maps_.dimension(1));
  assert(y.dimension(1) == maps_.dimension(2));
  assert(y.dimension(2) == maps_.dimension(3));
  y.device(Threads::GlobalDevice()) = ConjugateSum(x.slice(left_, size_), maps_);
}

void SenseBasisOp::AdjA(Input const &x, Input &y) const
{
  y.device(Threads::GlobalDevice()) = x;
}
