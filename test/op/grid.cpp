#include "../../src/op/grid.h"
#include "../../src/sdc.h"
#include "../../src/tensorOps.h"
#include "../../src/traj_spirals.h"
#include "../../src/trajectory.h"

#include <catch2/catch.hpp>

TEST_CASE("ops-grid", "[ops]")
{
  Index const M = 16;
  float const os = 2.f;
  Info const info{
    .type = Info::Type::ThreeD,
    .matrix = Eigen::Array3l::Constant(M),
    .channels = 1,
    .read_points = Index(os * M / 2),
    .spokes = Index(M * M),
    .volumes = 1,
    .echoes = 1,
    .tr = 1.f,
    .voxel_size = Eigen::Array3f::Constant(1.f),
    .origin = Eigen::Array3f::Constant(0.f),
    .direction = Eigen::Matrix3f::Identity()};
  auto const points = ArchimedeanSpiral(info.read_points, info.spokes);
  Trajectory const traj(info, points);
  R2 const sdc = SDC::Pipe(traj, true, os);

  /* I don't think the classic Dot test from PyLops is applicable to gridding,
   * because it would not be correct to have a random radial k-space. The k0
   * samples should all be the same, not random. Hence instead I calculate
   * y = Adj*A*x for NN and then check if Dot(x,y) is the same as Dot(y,y).
   * Can't check for y = x because samples not on the radial spokes will be
   * missing, and checking for KB kernel is not valid because the kernel blurs
   * the grid.
   */
  SECTION("NN-Dot")
  {
    auto const nn = make_kernel("NN", info.type, os);
    auto const m1 = traj.mapping(1, os);
    auto grid = make_grid(nn.get(), m1, false);
    grid->setSDC(sdc);
    auto const dims = grid->inputDimensions();
    Cx5 x(dims), y(dims);
    Cx3 r(info.channels, info.read_points, info.spokes);

    x.setRandom();
    r = grid->A(x);
    y = grid->Adj(r);
    auto const xy = Dot(x, y);
    auto const yy = Dot(y, y);
    CHECK(std::abs((yy - xy) / (yy + xy + 1.e-15f)) == Approx(0).margin(1.e-6));
  }
}