#pragma once

#include "operator.hpp"
#include "tensorOps.hpp"

namespace rl {

template <typename Scalar_, size_t InRank_, size_t OutRank_ = InRank_>
struct TensorOperator : Op::Operator<Scalar_>
{
  using Scalar = Scalar_;
  using Base = Op::Operator<Scalar>;
  static const size_t InRank = InRank_;
  using InTensor = Eigen::Tensor<Scalar, InRank>;
  using InMap = Eigen::TensorMap<InTensor>;
  using InCMap = Eigen::TensorMap<InTensor const>;
  using InDims = typename InTensor::Dimensions;
  static const size_t OutRank = OutRank_;
  using OutTensor = Eigen::Tensor<Scalar, OutRank>;
  using OutMap = Eigen::TensorMap<OutTensor>;
  using OutCMap = Eigen::TensorMap<OutTensor const>;
  using OutDims = typename OutTensor::Dimensions;

  InDims ishape;
  OutDims oshape;

  TensorOperator(std::string const &name, InDims const xd, OutDims const yd)
    : Op::Operator<Scalar>{name}
    , ishape{xd}
    , oshape{yd}
  {
    Log::Print<Log::Level::Debug>(FMT_STRING("{} created. Input dims {} Output dims {}"), name, ishape, oshape);
  }

  virtual ~TensorOperator(){};

  virtual auto rows() const -> Index { return Product(oshape); }
  virtual auto cols() const -> Index { return Product(ishape); }

  void forward(typename Base::CMap const &x, typename Base::Map &y) const final
  {
    InCMap xm(x.data(), ishape);
    OutMap ym(y.data(), oshape);
    forward(xm, ym);
  }

  void adjoint(typename Base::CMap const &y, typename Base::Map &x) const final
  {
    OutCMap ym(y.data(), oshape);
    InMap xm(x.data(), ishape);
    adjoint(ym, xm);
  }

  virtual auto forward(InTensor const &x) const -> OutTensor
  {
    InCMap xm(x.data(), ishape);
    OutTensor y(oshape);
    OutMap ym(y.data(), oshape);
    forward(xm, ym);
    return y;
  }

  virtual auto adjoint(OutTensor const &y) const -> InTensor
  {
    OutCMap ym(y.data(), oshape);
    InTensor x(ishape);
    InMap xm(x.data(), ishape);
    adjoint(ym, xm);
    return x;
  }

  virtual void forward(InCMap const &x, OutMap &y) const;
  virtual void adjoint(OutCMap const &y, InMap &x) const;

  auto startForward(InCMap const &x) const
  {
    if (x.dimensions() != ishape) {
      Log::Fail("{} forward dims were: {} expected: {}", this->name, x.dimensions(), ishape);
    }
    if (Log::CurrentLevel() == Log::Level::Debug) {
      Log::Print<Log::Level::Debug>(FMT_STRING("{} forward started. Norm {}"), this->name, Norm(x));
    }
    return Log::Now();
  }

  void finishForward(OutMap const &y, Log::Time const start) const
  {
    if (Log::CurrentLevel() == Log::Level::Debug) {
      Log::Print<Log::Level::Debug>(
        FMT_STRING("{} forward finished. Took {}. Norm {}."), this->name, Log::ToNow(start), Norm(y));
    }
  }

  auto startAdjoint(OutCMap const &y) const
  {
    if (y.dimensions() != oshape) {
      Log::Fail("{} adjoint dims were: {} expected: {}", this->name, y.dimensions(), oshape);
    }
    if (Log::CurrentLevel() == Log::Level::Debug) {
      Log::Print<Log::Level::Debug>(FMT_STRING("{} adjoint started. Norm {}"), this->name, Norm(y));
    }
    return Log::Now();
  }

  void finishAdjoint(InMap const &x, Log::Time const start) const
  {
    if (Log::CurrentLevel() == Log::Level::Debug) {
      Log::Print<Log::Level::Debug>(
        FMT_STRING("{} adjoint finished. Took {}. Norm {}"), this->name, Log::ToNow(start), Norm(x));
    }
  }
};

#define OP_INHERIT(SCALAR, INRANK, OUTRANK)                                                                                    \
  using Parent = TensorOperator<SCALAR, INRANK, OUTRANK>;                                                                      \
  using Scalar = typename Parent::Scalar;                                                                                      \
  static const size_t InRank = Parent::InRank;                                                                                 \
  using InTensor = typename Parent::InTensor;                                                                                  \
  using InMap = typename Parent::InMap;                                                                                        \
  using InCMap = typename Parent::InCMap;                                                                                      \
  using InDims = typename Parent::InDims;                                                                                      \
  using Parent::ishape;                                                                                                        \
  static const size_t OutRank = Parent::OutRank;                                                                               \
  using OutTensor = typename Parent::OutTensor;                                                                                \
  using OutMap = typename Parent::OutMap;                                                                                      \
  using OutCMap = typename Parent::OutCMap;                                                                                    \
  using OutDims = typename Parent::OutDims;                                                                                    \
  using Parent::oshape;

#define OP_DECLARE()                                                                                                           \
  void forward(InCMap const &x, OutMap &y) const;                                                                              \
  void adjoint(OutCMap const &y, InMap &x) const;                                                                              \
  using Parent::forward;                                                                                                       \
  using Parent::adjoint;

template <typename Scalar_, size_t Rank>
struct TensorIdentity : TensorOperator<Scalar_, Rank, Rank>
{
  OP_INHERIT(Scalar_, Rank, Rank)
  TensorIdentity(Sz<Rank> dims)
    : Parent("Identity", dims, dims)
  {
  }

  void forward(InCMap const &x, OutMap &y) const
  {
    auto const time = Parent::startForward(x);
    y = x;
    Parent::finishAdjoint(y, time);
  }

  void adjoint(OutCMap const &y, InMap &x) const
  {
    auto const time = Parent::startAdjoint(y);
    x = y;
    Parent::finishAdjoint(x, time);
  }
};

} // namespace rl
