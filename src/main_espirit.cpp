#include "types.h"

#include "cropper.h"
#include "espirit.h"
#include "fft_plan.h"
#include "filter.h"
#include "io.h"
#include "log.h"
#include "op/grid.h"
#include "parse_args.h"
#include "sense.h"
#include "tensorOps.h"

int main_espirit(args::Subparser &parser)
{
  CORE_RECON_ARGS;

  args::ValueFlag<long> volume(
    parser, "VOL", "Take SENSE maps from this volume (default last)", {"volume"}, -1);
  args::ValueFlag<float> fov(parser, "FOV", "FoV in mm (default header value)", {"fov"}, -1);
  args::ValueFlag<long> kRad(parser, "RAD", "Kernel radius (default 4)", {"kRad", 'k'}, 4);
  args::ValueFlag<long> calRad(
    parser, "RAD", "Additional calibration radius (default 1)", {"calRad", 'c'}, 1);
  args::ValueFlag<float> thresh(
    parser, "T", "Variance threshold to retain kernels (0.015)", {"thresh"}, 0.015);
  args::ValueFlag<float> res(
    parser, "R", "Resolution for initial gridding (default 8 mm)", {"res", 'r'}, 8.f);
  args::Flag nifti(parser, "NIFTI", "Write output to nifti instead of .h5", {"nii"});

  Log log = ParseCommand(parser, iname);
  FFT::Start(log);

  HD5::Reader reader(iname.Get(), log);
  auto const traj = reader.readTrajectory();
  auto const &info = traj.info();
  log.info(FMT_STRING("Cropping data to {} mm effective resolution"), res.Get());
  auto gridder = make_grid(traj, osamp.Get(), kernel.Get(), fastgrid, log, res);
  gridder->setSDC(SDC::Pipe(traj, true, osamp.Get(), log));
  long const totalCalRad = kRad.Get() + calRad.Get() + (info.spokes_lo ? 0 : info.read_gap);
  Cropper cropper(info, gridder->mapping().cartDims, fov.Get(), log);
  Cx4 sense = cropper.crop4(ESPIRIT(
    gridder,
    reader.noncartesian(ValOrLast(volume.Get(), info.volumes)),
    kRad.Get(),
    totalCalRad,
    info.read_gap,
    thresh.Get(),
    log));

  auto const fname = OutName(iname.Get(), oname.Get(), "espirit", "h5");
  HD5::Writer writer(fname, log);
  writer.writeInfo(info);
  writer.writeSENSE(sense);

  FFT::End(log);
  return EXIT_SUCCESS;
}
