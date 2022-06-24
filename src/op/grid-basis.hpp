#pragma once

#include "grid-base.hpp"

template <int IP, int TP>
struct GridBasis final : SizedGrid<IP, TP>
{
  using typename SizedGrid<IP, TP>::Input;
  using typename SizedGrid<IP, TP>::Output;

  GridBasis(
    SizedKernel<IP, TP> const *k, Mapping const &mapping, Index const nC, R2 const &basis, bool const unsafe)
    : SizedGrid<IP, TP>(k, mapping, nC, basis.dimension(1), unsafe)
    , basis_{basis}
  {
    Log::Debug(FMT_STRING("GridBasis<{},{}>, dims"), IP, TP, this->inputDimensions());
  }

  R2 const &basis() const
  {
    return basis_;
  }

  Output A(Input const &cart) const
  {
    if (cart.dimensions() != this->inputDimensions()) {
      Log::Fail(FMT_STRING("Cartesian k-space dims {} did not match {}"), cart.dimensions(), this->inputDimensions());
    }
    Output noncart(this->outputDimensions());
    noncart.setZero();
    Index const nC = this->inputDimensions()[0];
    Index const nB = this->inputDimensions()[1];
    auto const scale = this->mapping_.scale * sqrt(basis_.dimension(0));
    auto grid_task = [&](Index const ii) {
      auto const si = this->mapping_.sortedIndices[ii];
      auto const c = this->mapping_.cart[si];
      auto const n = this->mapping_.noncart[si];
      auto const k = this->kernel_->k(this->mapping_.offset[si]);
      R1 const b = basis_.chip<0>(n.spoke % basis_.dimension(0));
      Index const stX = c.x - ((IP - 1) / 2);
      Index const stY = c.y - ((IP - 1) / 2);
      Index const stZ = c.z - ((TP - 1) / 2);
      Cx1 sum(nC);
      sum.setZero();
      for (Index iz = 0; iz < TP; iz++) {
        for (Index iy = 0; iy < IP; iy++) {
          for (Index ix = 0; ix < IP; ix++) {
            float const kval = k(ix, iy, iz) * scale;
            for (Index ib = 0; ib < nB; ib++) {
              float const bval = b(ib) * kval;
              for (Index ic = 0; ic < nC; ic++) {
                sum(ic) += cart(ic, ib, stX + ix, stY + iy, stZ + iz) * bval;
              }
            }
          }
        }
      }
      noncart.chip(n.spoke, 2).chip(n.read, 1) = sum;
    };
    auto const &start = Log::Now();
    noncart.setZero();
    Threads::For(grid_task, this->mapping_.cart.size(), "Basis Gridding Forward");
    Log::Debug("Cart -> Non-cart: {}", Log::ToNow(start));
    return noncart;
  }

  Input &Adj(Output const &noncart) const
  {
    Log::Debug("Grid with Basis Adjoint");
    if (noncart.dimensions() != this->outputDimensions()) {
      Log::Fail(FMT_STRING("Noncartesian k-space dims {} did not match {}"), noncart.dimensions(), this->outputDimensions());
    }
    auto const &cdims = this->inputDimensions();
    Index const nC = cdims[0];
    Index const nB = cdims[1];
    auto const scale = this->mapping_.scale * sqrt(basis_.dimension(0));
    auto dev = Threads::GlobalDevice();
    Index const nThreads = dev.numThreads();
    std::vector<Cx5> threadSpaces(nThreads);
    std::vector<Index> minZ(nThreads, 0L), szZ(nThreads, 0L);
    auto grid_task = [&](Index const lo, Index const hi, Index const ti) {
      if (this->safe_) {
        Index const maxZ = this->mapping_.cart[this->mapping_.sortedIndices[hi - 1]].z + (TP / 2);
        minZ[ti] = this->mapping_.cart[this->mapping_.sortedIndices[lo]].z - ((TP - 1) / 2);
        szZ[ti] = maxZ - minZ[ti] + 1;
        threadSpaces[ti].resize(nC, nB, cdims[2], cdims[3], szZ[ti]);
        threadSpaces[ti].setZero();
      }
      Cx5 &out = this->safe_ ? threadSpaces[ti] : *(this->ws_);

      for (auto ii = lo; ii < hi; ii++) {
        if (ti == 0) {
          Log::Tick();
        }
        auto const si = this->mapping_.sortedIndices[ii];
        auto const c = this->mapping_.cart[si];
        auto const n = this->mapping_.noncart[si];
        auto const k = this->kernel_->k(this->mapping_.offset[si]);
        R1 const b = basis_.chip<0>(n.spoke % basis_.dimension(0));

        Index const stX = c.x - ((IP - 1) / 2);
        Index const stY = c.y - ((IP - 1) / 2);
        Index const stZ = c.z - ((TP - 1) / 2) - minZ[ti];
        for (Index iz = 0; iz < TP; iz++) {
          for (Index iy = 0; iy < IP; iy++) {
            for (Index ix = 0; ix < IP; ix++) {
              for (Index ib = 0; ib < nB; ib++) {
                float const kval = k(ix, iy, iz) * scale;
                for (Index ic = 0; ic < nC; ic++) {
                  out(ic, ib, stX + ix, stY + iy, stZ + iz) += noncart(ic, n.read, n.spoke) * b(ib) * kval;
                }
              }
            }
          }
        }
      }
    };

    auto const start = Log::Now();
    this->ws_->setZero();
    Log::StartProgress(this->mapping_.cart.size() / dev.numThreads(), "Adjoint Basis Gridding");
    Threads::RangeFor(grid_task, this->mapping_.cart.size());
    Log::StopProgress();
    Log::Debug("Basis Non-cart -> Cart: {}", Log::ToNow(start));
    if (this->safe_) {
      Log::Debug(FMT_STRING("Combining thread workspaces..."));
      auto const start2 = Log::Now();
      Sz5 st{0, 0, 0, 0, 0};
      Sz5 sz{nC, nB, cdims[2], cdims[3], 0};
      for (Index ti = 0; ti < nThreads; ti++) {
        if (szZ[ti]) {
          st[4] = minZ[ti];
          sz[4] = szZ[ti];
          this->ws_->slice(st, sz).device(dev) += threadSpaces[ti];
        }
      }
      Log::Debug(FMT_STRING("Combining took: {}"), Log::ToNow(start2));
    }

    return *(this->ws_);
  }

private:
  using FixIn = Eigen::type2index<IP>;
  using FixThrough = Eigen::type2index<TP>;

  R2 basis_;
};
