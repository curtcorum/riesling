#include "recon.hpp"

#include "op/compose.hpp"
#include "op/nufft.hpp"
#include "sdc.hpp"
#include "sense.hpp"

namespace rl {

auto make_recon(
  CoreOpts &coreOpts,
  SDC::Opts &sdcOpts,
  SENSE::Opts &senseOpts,
  Trajectory const &traj,
  bool const toeplitz,
  HD5::Reader &reader) -> std::shared_ptr<ReconOp>
{
  auto const basis = ReadBasis(coreOpts.basisFile.Get());
  auto sense = std::make_shared<SenseOp>(
    SENSE::Choose(senseOpts, coreOpts, traj, reader), basis ? basis.value().dimension(0) : 1);
  auto const sdc = SDC::Choose(sdcOpts, sense->nChannels(), traj, coreOpts.ktype.Get(), coreOpts.osamp.Get());
  auto nufft = make_nufft(
    traj, coreOpts.ktype.Get(), coreOpts.osamp.Get(), sense->nChannels(), sense->mapDimensions(), basis, sdc, toeplitz);
  return std::make_shared<ReconOp>(sense, nufft);
}

} // namespace rl