#include "types.hpp"

#include "basis/basis.hpp"
#include "io/hd5.hpp"
#include "log.hpp"
#include "parse_args.hpp"

void main_echoes(args::Subparser &parser)
{
  args::Positional<std::string> oname(parser, "OUTPUT", "Name for the basis file");

  args::ValueFlag<Index> nS(parser, "S", "Total samples", {"samples", 's'});
  args::ValueFlag<Index> nK(parser, "K", "Samples to keep", {"keep", 'k'});
  args::ValueFlag<Index> nE(parser, "E", "Number of echoes", {"echoes", 'e'});

  ParseCommand(parser);
  if (!oname) { throw args::Error("No output filename specified"); }

  Index sampPerEcho = nS.Get() / nE.Get();
  rl::Log::Print("Echoes {} Samples {} Keep {} Samples-per-echo {}", nE.Get(), nS.Get(), nK.Get(), sampPerEcho);
  rl::Re3 basis(nE.Get(), nK.Get(), 1);
  basis.setZero();
  for (Index is = 0; is < nK.Get(); is++) {
    Index const ind = is / sampPerEcho;
    basis(ind, is, 0) = 1.f;
  }

  rl::HD5::Writer writer(oname.Get());
  writer.writeTensor(rl::HD5::Keys::Basis, rl::Sz3{basis.dimension(0), 1, basis.dimension(1)}, basis.data(), rl::HD5::Dims::Basis);
}
