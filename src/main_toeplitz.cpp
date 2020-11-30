#include "types.h"

#include "cg.h"
#include "cropper.h"
#include "fft.h"
#include "filter.h"
#include "gridder.h"
#include "io_hd5.h"
#include "io_nifti.h"
#include "log.h"
#include "parse_args.h"
#include "sense.h"
#include "threads.h"
#include <filesystem>

int main_toeplitz(args::Subparser &parser)
{
  args::Positional<std::string> fname(parser, "FILE", "HD5 file to recon");
  args::ValueFlag<std::string> oname(parser, "OUTPUT", "Override output name", {'o', "out"});

  args::ValueFlag<long> volume(parser, "VOLUME", "Only recon this volume", {"vol"}, -1);
  args::ValueFlag<float> crop(
      parser, "CROP SIZE", "Crop extent in mm (default 220)", {"crop"}, 240.f);
  args::Flag est_dc(parser, "ESTIMATE DC", "Estimate DC weights instead of analytic", {"est_dc"});
  args::ValueFlag<float> dc_exp(
      parser, "DC Exponent", "Density-Compensation Exponent (default 1.0)", {'d', "dce"}, 1.0f);
  args::ValueFlag<float> osamp(
      parser, "GRID OVERSAMPLE", "Oversampling factor for gridding, default 2", {'g', "grid"}, 2.f);
  args::ValueFlag<long> sense_vol(
      parser, "SENSE VOLUME", "Take SENSE maps from this volume", {"sense_vol"}, 0);
  args::ValueFlag<float> thr(
      parser, "TRESHOLD", "Threshold for termination (1e-10)", {"thresh"}, 1.e-10);
  args::ValueFlag<long> its(
      parser, "MAX ITS", "Maximum number of iterations (16)", {'i', "max_its"}, 16);

  args::ValueFlag<float> tukey_s(
      parser, "TUKEY START", "Start-width of Tukey filter", {"tukey_start"}, 1.0f);
  args::ValueFlag<float> tukey_e(
      parser, "TUKEY END", "End-width of Tukey filter", {"tukey_end"}, 1.0f);
  args::ValueFlag<float> tukey_h(
      parser, "TUKEY HEIGHT", "End height of Tukey filter", {"tukey_height"}, 0.0f);
  args::Flag magnitude(parser, "MAGNITUDE", "Output magnitude images only", {"magnitude"});
  Log log = ParseCommand(parser, fname);
  FFTStart(log);
  RadialReader reader(fname.Get(), log);
  auto const &info = reader.info();
  Cx3 rad_ks = info.radialVolume();

  R3 const trajectory = reader.readTrajectory();
  Gridder gridder(info, trajectory, osamp.Get(), log);
  gridder.setDCExponent(dc_exp.Get());
  if (est_dc) {
    gridder.estimateDC();
  }

  Cx4 grid = gridder.newGrid();
  Cropper iter_cropper(info, gridder.gridDims(), crop.Get(), log);
  FFT3N fft(grid, log);

  reader.readData(SenseVolume(sense_vol, info.volumes), rad_ks);
  Cx4 const sense = iter_cropper.crop4(SENSE(info, trajectory, osamp.Get(), rad_ks, log));

  Cx2 ones(info.read_points, info.spokes_total());
  ones.setConstant({1.0f});
  Cx3 transfer(gridder.gridDims());
  transfer.setZero();
  gridder.toCartesian(ones, transfer);

  SystemFunction toe = [&](Cx3 const &x, Cx3 &y) {
    auto const start = log.start_time();
    grid.device(Threads::GlobalDevice()) = grid.constant(0.f);
    iter_cropper.crop4(grid).device(Threads::GlobalDevice()) = sense * tile(x, info.channels);
    fft.forward();
    grid.device(Threads::GlobalDevice()) = grid * tile(transfer, info.channels);
    fft.reverse();
    y.device(Threads::GlobalDevice()) = (iter_cropper.crop4(grid) * sense.conjugate()).sum(Sz1{0});
    log.stop_time(start, "Total system time");
  };

  DecodeFunction dec = [&](Cx3 const &radial, Cx3 &out) {
    auto const &start = log.start_time();
    out.setZero();
    grid.setZero();
    gridder.toCartesian(radial, grid);
    fft.reverse();
    out.device(Threads::GlobalDevice()) =
        (iter_cropper.crop4(grid) * sense.conjugate()).sum(Sz1{0});
    log.stop_time(start, "Total decode time");
  };

  Cropper out_cropper(info, iter_cropper.size(), -1, log);
  Cx4 out = out_cropper.newSeries(info.volumes);
  Cx3 vol = iter_cropper.newImage();
  auto const &all_start = log.start_time();
  for (auto const &iv : WhichVolumes(volume.Get(), info.volumes)) {
    auto const &vol_start = log.start_time();
    reader.readData(iv, rad_ks);
    dec(rad_ks, vol); // Initialize
    cg(toe, its.Get(), thr.Get(), vol, log);

    if (tukey_s || tukey_e || tukey_h) {
      ImageTukey(tukey_s.Get(), tukey_e.Get(), tukey_h.Get(), vol, log);
    }

    out.chip(iv, 3) = out_cropper.crop3(vol);
    log.stop_time(vol_start, "Volume took");
  }
  log.stop_time(all_start, "All volumes took");
  auto const ofile = OutName(fname, oname, "toe");
  if (magnitude) {
    WriteVolumes(info, R4(out.abs()), volume.Get(), ofile, log);
  } else {
    WriteVolumes(info, out, volume.Get(), ofile, log);
  }
  FFTEnd(log);
  return EXIT_SUCCESS;
}
