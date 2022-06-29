#include "mapping.h"

#include <cfenv>
#include <cmath>

// Helper function to convert a floating-point vector-like expression to integer values
template <typename T>
inline decltype(auto) nearby(T &&x)
{
  return x.array().unaryExpr([](float const &e) { return std::nearbyint(e); });
}

// Helper function to get a "good" FFT size. Empirical rule of thumb - multiples of 8 work well
inline Index fft_size(float const x)
{
  if (x > 8.f) {
    return (std::lrint(x) + 7L) & ~7L;
  } else {
    return (Index)std::ceil(x);
  }
}

Mapping::Mapping(Trajectory const &traj, Kernel const *k, float const os, Index const bucketSz, Index const read0)
{
  Info const &info = traj.info();
  Index const gridSz = fft_size(info.matrix.maxCoeff() * os);
  Log::Print(FMT_STRING("Mapping to grid size {}"), gridSz);

  cartDims = info.type == Info::Type::ThreeD ? Sz3{gridSz, gridSz, gridSz} : Sz3{gridSz, gridSz, info.matrix[2]};
  noncartDims = Sz2{info.read_points, info.spokes};
  scale = sqrt(info.type == Info::Type::ThreeD ? pow(os, 3) : pow(os, 2));
  frames = info.frames;
  frameWeights = Eigen::ArrayXf(frames);

  Index const nbX = std::ceil(cartDims[0] / float(bucketSz));
  Index const nbY = std::ceil(cartDims[1] / float(bucketSz));
  Index const nbZ = std::ceil(cartDims[2] / float(bucketSz));
  Index const nB = nbX * nbY * nbZ;
  buckets.reserve(nB);
  Index const IP = k->inPlane();
  Index const TP = k->throughPlane();
  for (Index iz = 0; iz < nbZ; iz++) {
    for (Index iy = 0; iy < nbY; iy++) {
      for (Index ix = 0; ix < nbX; ix++) {
        buckets.push_back(Bucket{
          Sz3{ix * bucketSz - (IP / 2), iy * bucketSz - (IP / 2), iz * bucketSz - (TP / 2)},
          Sz3{
            std::min((ix + 1) * bucketSz, cartDims[0]) + (IP / 2),
            std::min((iy + 1) * bucketSz, cartDims[1]) + (IP / 2),
            std::min((iz + 1) * bucketSz, cartDims[2]) + (TP / 2)}});
      }
    }
  }

  Log::Print("Calculating mapping");
  std::fesetround(FE_TONEAREST);
  float const maxRad = (gridSz / 2) - 1.f;
  Size3 const center(cartDims[0] / 2, cartDims[1] / 2, cartDims[2] / 2);
  int32_t index = 0;
  for (int32_t is = 0; is < info.spokes; is++) {
    auto const fr = traj.frames()(is);
    if ((fr >= 0) && (fr < info.frames)) {
      for (int16_t ir = read0; ir < info.read_points; ir++) {
        NoncartesianIndex const nc{.spoke = is, .read = ir};
        Point3 const xyz = traj.point(ir, is, maxRad);
        if (xyz.array().isFinite().all()) { // Allow for NaNs in trajectory for blanking
          Point3 const gp = nearby(xyz);
          Size3 const ijk = center + Size3(gp.cast<int16_t>());
          auto const off = xyz - gp.cast<float>().matrix();

          cart.push_back(CartesianIndex{ijk(0), ijk(1), ijk(2)});
          offset.push_back(off);
          noncart.push_back(nc);
          frame.push_back(fr);

          // Calculate bucket
          Index const ix = ijk[0] / bucketSz;
          Index const iy = ijk[1] / bucketSz;
          Index const iz = ijk[2] / bucketSz;
          Index const ib = ix + nbX * (iy + (nbY * iz));
          buckets[ib].indices.push_back(index);
          frameWeights[fr] += 1;
          index++;
        }
      }
    }
  }

  Log::Print("Removing empty buckets");

  Index const eraseCount = std::erase_if(buckets, [](Bucket const &b) { return b.empty(); });

  Log::Print("Removed {} empty buckets, {} remaining", eraseCount, buckets.size());
  Log::Print("Total points {}", std::accumulate(buckets.begin(), buckets.end(), 0L, [](Index sum, Bucket const &b) {
               return b.indices.size() + sum;
             }));

  frameWeights = frameWeights.maxCoeff() / frameWeights;
  Log::Print(FMT_STRING("Frame weights: {}"), frameWeights.transpose());
}