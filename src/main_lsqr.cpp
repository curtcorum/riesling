#include "types.h"

#include "log.h"
#include "lsqr.hpp"
#include "op/recon.hpp"
#include "parse_args.h"
#include "sdc.h"
#include "sense.h"
#include "tensorOps.h"
#include "threads.h"

int main_lsqr(args::Subparser &parser)
{
  COMMON_RECON_ARGS;
  COMMON_SENSE_ARGS;
  args::ValueFlag<std::string> basisFile(parser, "BASIS", "Read basis from file", {"basis", 'b'});
  args::ValueFlag<float> lsq_thr(parser, "T", "LSQ threshold (1e-10)", {"lsq_thresh"}, 1.e-10);
  args::ValueFlag<Index> lsq_its(parser, "N", "LSQ iterations (8)", {'i', "lsq_its"}, 8);

  ParseCommand(parser, iname);

  HD5::RieslingReader reader(iname.Get());
  Trajectory const traj = reader.trajectory();
  Info const &info = traj.info();

  auto const kernel = make_kernel(ktype.Get(), info.type, osamp.Get());
  auto const mapping = traj.mapping(kernel->inPlane(), osamp.Get());
  auto gridder = make_grid(kernel.get(), mapping, fastgrid);
  auto const sdc = SDC::Choose(sdcType.Get(), sdcPow.Get(), traj, osamp.Get());
  Cx4 senseMaps = sFile ? LoadSENSE(sFile.Get())
                        : SelfCalibration(
                            info,
                            gridder.get(),
                            iter_fov.Get(),
                            sRes.Get(),
                            sReg.Get(),
                            sdc->apply(reader.noncartesian(ValOrLast(sVol.Get(), info.volumes))));

  if (basisFile) {
    HD5::Reader basisReader(basisFile.Get());
    R2 const basis = basisReader.readTensor<R2>(HD5::Keys::Basis);
    gridder = make_grid_basis(kernel.get(), gridder->mapping(), basis, fastgrid);
  }
  ReconOp recon(gridder.get(), senseMaps);
  auto sz = recon.inputDimensions();
  Cropper out_cropper(info, LastN<3>(sz), out_fov.Get());
  Cx4 vol(sz);
  Sz3 outSz = out_cropper.size();
  Cx4 cropped(sz[0], outSz[0], outSz[1], outSz[2]);
  Cx5 out(sz[0], outSz[0], outSz[1], outSz[2], info.volumes);
  auto const &all_start = Log::Now();
  for (Index iv = 0; iv < info.volumes; iv++) {
    auto const &vol_start = Log::Now();
    vol = lsqr(lsq_its.Get(), lsq_thr.Get(), recon, sdc, reader.noncartesian(iv));
    cropped = out_cropper.crop4(vol);
    out.chip<4>(iv) = cropped;
    Log::Print(FMT_STRING("Volume {}: {}"), iv, Log::ToNow(vol_start));
  }
  Log::Print(FMT_STRING("All Volumes: {}"), Log::ToNow(all_start));
  auto const fname = OutName(iname.Get(), oname.Get(), "lsqr", "h5");
  HD5::Writer writer(fname);
  writer.writeTrajectory(traj);
  writer.writeTensor(out, "image");

  return EXIT_SUCCESS;
}
