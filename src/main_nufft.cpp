#include "types.h"

#include "io.h"
#include "log.h"
#include "op/nufft.hpp"
#include "parse_args.h"
#include "sdc.h"
#include "threads.h"

int main_nufft(args::Subparser &parser)
{
  COMMON_RECON_ARGS;
  args::Flag forward(parser, "F", "Apply forward (default reverse)", {'f', "fwd"});
  args::ValueFlag<std::string> dset(
    parser, "D", "Dataset name (image/noncartesian)", {'d', "dset"});
  ParseCommand(parser, iname);
  FFT::Start();
  HD5::RieslingReader reader(iname.Get());
  auto const traj = reader.trajectory();
  auto const info = traj.info();
  auto const kernel = make_kernel(ktype.Get(), info.type, osamp.Get());
  auto const mapping = traj.mapping(kernel->inPlane(), osamp.Get());
  auto gridder = make_grid(kernel.get(), mapping, fastgrid);
  if (!forward) {
    gridder->setSDC(SDC::Choose(sdc.Get(), traj, osamp.Get()));
    gridder->setSDCPower(sdcPow.Get());
  }

  NUFFTOp nufft(LastN<3>(info.matrix), gridder.get());
  Cx5 channels{nufft.inputDimensions()};
  Cx3 noncart{nufft.outputDimensions()};
  HD5::Writer writer(OutName(iname.Get(), oname.Get(), "nufft", "h5"));
  writer.writeTrajectory(traj);
  auto const start = Log::Now();
  if (forward) {
    std::string const name = dset ? dset.Get() : "image";
    auto const input = reader.readTensor<Cx4>(name);
    channels = input.reshape(
      Sz5{input.dimension(0), 1, input.dimension(1), input.dimension(2), input.dimension(3)});
    noncart = nufft.A(channels);
    writer.writeTensor(noncart, "nufft-forward");
    Log::Print("Forward NUFFT took {}", Log::ToNow(start));
  } else {
    std::string const name = dset ? dset.Get() : "nufft-forward";
    noncart = reader.readTensor<Cx3>(name);
    channels = nufft.Adj(noncart);
    Cx4 output = channels.reshape(Sz4{
      channels.dimension(0), channels.dimension(2), channels.dimension(3), channels.dimension(4)});
    writer.writeTensor(output, "nufft-backward");
    Log::Print("Backward NUFFT took {}", Log::ToNow(start));
  }

  FFT::End();
  return EXIT_SUCCESS;
}
