#pragma once

#include "info.hpp"
#include "io/reader.hpp"
#include "io/writer.hpp"
#include "log.hpp"
#include "types.hpp"

namespace rl {

struct Trajectory
{
  Trajectory();
  Trajectory(Info const &info, Re3 const &points);

  Trajectory(HD5::Reader const &reader);
  void write(HD5::Writer &writer) const;

  auto nDims() const -> Index;
  auto nSamples() const -> Index;
  auto nTraces() const -> Index;
  auto info() const -> Info const &;
  auto matrix(float const fov = -1.f) const -> Sz3;
  auto point(int16_t const sample, int32_t const trace) const -> Re1;
  auto points() const -> Re3 const &;
  auto downsample(float const res, Index const lores, bool const shrink) const -> std::tuple<Trajectory, Index, Index>;
  auto downsample(Cx4 const &ks, float const res, Index const lores, bool const shrink) const
    -> std::tuple<Trajectory, Cx4>;
  auto downsample(Cx5 const &ks, float const res, Index const lores, bool const shrink) const
    -> std::tuple<Trajectory, Cx5>;

private:
  void init();

  Info info_;
  Re3 points_;
};

} // namespace rl
