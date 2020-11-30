#pragma once

#include "filter.h"
#include "interpolator.h"
#include "log.h"
#include "radial.h"
#include "threads.h"

#include <vector>

/** Transforms between non-cartesian and cartesian grids
 */
struct Gridder
{
  /** Constructs the Gridder with a default Cartesian grid
   *
   * @param info   The Radial info struct
   * @param traj   The trajectory stored as X,Y,Z positions in k-space relative to k-max
   * @param os     Oversampling factor
   * @param log    Logging object
   */
  Gridder(RadialInfo const &info, R3 const &traj, float const os, Log &log);

  /** Constructs the Gridder with the specified resoluion
   *
   * @param info   The Radial info struct
   * @param traj   The trajectory stored as X,Y,Z positions in k-space relative to k-max
   * @param os     Oversampling factor
   * @param res    Desired effective resolution
   * @param shrink Shrink the grid to fit only the desired resolution portion
   * @param log    Logging object
   */
  Gridder(
      RadialInfo const &info,
      R3 const &traj,
      float const os,
      float const res,
      bool const shrink,
      Log &log);

  virtual ~Gridder() = default;

  long gridRadius() const;      //!< Returns the current maximum gridding radius as a k-space index
  long gridSize() const;        //!< Returns the size of the grid in one dimension
  Dims3 const gridDims() const; //!< Returns the dimensions of the oversampled grid
  Cx4 newGrid() const;
  Cx3 newGrid1() const;

  void setDCExponent(float const dce); //!< Sets the exponent of the density compensation weights
  void estimateDC();                   //!< Iteratively estimate the density-compensation weights

  void toCartesian(Cx2 const &radial, Cx3 &cart) const; //!< Single-channel non-cartesian -> cart
  void toCartesian(Cx3 const &radial, Cx4 &cart) const; //!< Multi-channel non-cartesian -> cart
  void toRadial(Cx3 const &cart, Cx2 &radial) const;    //!< Single-channel cart -> non-cart
  void toRadial(Cx4 const &cart, Cx3 &radial) const;    //!< Multi-channel cart -> non-cart

private:
  struct CoordSet
  {
    Point3 cart;
    Size3 wrapped;
    Size2 radial;
  };
  void setup(R3 const &traj, float const res, bool const shrink);
  void analyticDC();

  RadialInfo const info_;
  std::vector<CoordSet> coords_;
  R2 DC_, merge_;
  float oversample_, dc_exp_;
  Log &log_;
  long highestReadIndex_, fullSize_, nominalSize_;
};
