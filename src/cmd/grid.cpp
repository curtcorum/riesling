#include "types.hpp"

#include "io/hd5.hpp"
#include "log.hpp"
#include "op/make_grid.hpp"
#include "parse_args.hpp"
#include "sdc.hpp"
#include "threads.hpp"
#include <filesystem>

using namespace rl;

int main_grid(args::Subparser &parser)
{
  CoreOpts coreOpts(parser);
  SDC::Opts sdcOpts(parser, "pipe");
  args::Flag fwd(parser, "", "Apply forward operation", {'f', "fwd"});
  args::Flag bucket(parser, "", "Use bucket gridder", {"bucket"});

  ParseCommand(parser, coreOpts.iname);
  HD5::Reader reader(coreOpts.iname.Get());
  Trajectory traj(reader);

  auto const basis = ReadBasis(coreOpts.basisFile.Get());

  HD5::Writer writer(OutName(coreOpts.iname.Get(), coreOpts.oname.Get(), "grid", "h5"));
  traj.write(writer);
  auto const start = Log::Now();
  if (fwd) {
    Cx5 cart = reader.readTensor<Cx5>(HD5::Keys::Cartesian);
    auto const gridder = make_grid<Cx, 3>(traj, coreOpts.ktype.Get(), coreOpts.osamp.Get(), cart.dimension(0), basis);
    auto const rad_ks = gridder->forward(cart);
    writer.writeTensor(
      HD5::Keys::Noncartesian, Sz5{rad_ks.dimension(0), rad_ks.dimension(1), rad_ks.dimension(2), 1, 1}, rad_ks.data());
    Log::Print("Wrote non-cartesian k-space. Took {}", Log::ToNow(start));
  } else {
    auto const noncart = reader.readSlab<Cx4>(HD5::Keys::Noncartesian, 0);
    Index const nC = noncart.dimension(0);
    Index const nS = noncart.dimension(3);
    auto const gridder = make_grid<Cx, 3>(traj, coreOpts.ktype.Get(), coreOpts.osamp.Get(), nC, basis);
    auto const sdc = SDC::Choose(sdcOpts, nC, traj, coreOpts.ktype.Get(), coreOpts.osamp.Get());
    Cx6 cart(AddBack(gridder->ishape, nS));
    for (Index is = 0; is < nS; is++) {
      Cx3 slice = noncart.chip<3>(is);
      slice = sdc->adjoint(slice);
      cart.chip<5>(is) = gridder->adjoint(slice);
    }
    writer.writeTensor(HD5::Keys::Cartesian, cart.dimensions(), cart.data());
    Log::Print("Wrote cartesian k-space. Took {}", Log::ToNow(start));
  }

  return EXIT_SUCCESS;
}
