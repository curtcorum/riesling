#pragma once

#include "log.hpp"
#include "types.hpp"

namespace rl {

namespace Op {

template <typename Scalar_ = Cx>
struct Operator
{
  using Scalar = Scalar_;
  using Vector = Eigen::Vector<Scalar, Eigen::Dynamic>;
  using Map = Eigen::Map<Vector>;
  using CMap = Eigen::Map<Vector const>;

  std::string name;
  Operator(std::string const &n);

  virtual auto rows() const -> Index = 0;
  virtual auto cols() const -> Index = 0;
  virtual void forward(CMap const &x, Map &y) const = 0;
  virtual void adjoint(CMap const &x, Map &y) const = 0;
  virtual auto forward(Vector const &x) const -> Vector;
  virtual auto adjoint(Vector const &y) const -> Vector;
  void forward(Vector const &x, Vector &y) const;
  void adjoint(Vector const &y, Vector &x) const;
};

template <typename Scalar = Cx>
struct Identity final : Operator<Scalar>
{
  using typename Operator<Scalar>::Map;
  using typename Operator<Scalar>::CMap;

  Identity(Index const s);

  auto rows() const -> Index;
  auto cols() const -> Index;
  void forward(CMap const &x, Map &y) const;
  void adjoint(CMap const &y, Map &x) const;

private:
  Index sz;
};

template <typename Scalar = Cx>
struct Scale final : Operator<Scalar>
{
  using typename Operator<Scalar>::Map;
  using typename Operator<Scalar>::CMap;

  Scale(Index const size, float const s);

  auto rows() const -> Index;
  auto cols() const -> Index;

  void forward(CMap const &x, Map &y) const;
  void adjoint(CMap const &y, Map &x) const;

  Index sz;
  float scale;
};

template <typename Scalar = Cx>
struct Concat final : Operator<Scalar>
{
  using Op = Operator<Scalar>;
  using typename Op::Vector;
  using typename Op::Map;
  using typename Op::CMap;

  std::shared_ptr<Op> a, b;

  Concat(std::shared_ptr<Op> a_, std::shared_ptr<Op> b_);

  auto rows() const -> Index;
  auto cols() const -> Index;

  void forward(CMap const &x, Map &y) const;
  void adjoint(CMap const &y, Map &x) const;
};

template <typename Scalar = Cx>
struct VStack final : Operator<Scalar>
{
  using typename Operator<Scalar>::Vector;
  using typename Operator<Scalar>::Map;
  using typename Operator<Scalar>::CMap;

  VStack(std::vector<std::shared_ptr<Operator<Scalar>>> const &o);

  auto rows() const -> Index;
  auto cols() const -> Index;

  void forward(CMap const &x, Map &y) const;
  void adjoint(CMap const &y, Map &x) const;

private:
  std::vector<std::shared_ptr<Operator<Scalar>>> ops;
};

}

} // namespace rl
