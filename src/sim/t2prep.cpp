#include "t2prep.hpp"

#include "unsupported/Eigen/MatrixFunctions"

namespace rl {

T2Prep::T2Prep(Settings const &s)
  : Sequence{s}
{
}

auto T2Prep::length() const -> Index { return settings.spg * settings.gps; }

auto T2Prep::parameters(Index const nsamp, std::vector<float> lo, std::vector<float> hi) const -> Eigen::ArrayXXf
{
  return Parameters::T1T2B1(nsamp, lo, hi);
}

auto T2Prep::simulate(Eigen::ArrayXf const &p) const -> Eigen::ArrayXf
{
  float const R1 = 1.f / p(0);
  float const R2 = 1.f / p(1);
  float const B1 = p(2);
  Eigen::ArrayXf dynamic(settings.spg * settings.gps);

  Eigen::Matrix2f E1, E2, Eramp, Essi, Erec;
  float const e1 = exp(-R1 * settings.TR);
  float const eramp = exp(-R1 * settings.Tramp);
  float const essi = exp(-R1 * settings.Tssi);
  float const erec = exp(-R1 * settings.Trec);
  E1 << e1, 1 - e1, 0.f, 1.f;
  E2 << exp(-R2 * settings.TE), 0.f, 0.f, 1.f;
  Eramp << eramp, 1 - eramp, 0.f, 1.f;
  Essi << essi, 1 - essi, 0.f, 1.f;
  Erec << erec, 1 - erec, 0.f, 1.f;

  float const cosa = cos(B1 * settings.alpha * M_PI / 180.f);
  float const sina = sin(B1 * settings.alpha * M_PI / 180.f);

  Eigen::Matrix2f A;
  A << cosa, 0.f, 0.f, 1.f;

  // Get steady state after prep-pulse for first segment
  Eigen::Matrix2f const seg = (Essi * Eramp * (E1 * A).pow(settings.spg) * Eramp).pow(settings.gps);
  Eigen::Matrix2f const SS = Essi * E2 * Erec * seg;
  float const m_ss = SS(0, 1) / (1.f - SS(0, 0));

  // Now fill in dynamic
  Index tp = 0;
  Eigen::Vector2f Mz{m_ss, 1.f};
  for (Index ig = 0; ig < settings.gps; ig++) {
    Mz = Eramp * Mz;
    for (Index ii = 0; ii < settings.spg; ii++) {
      dynamic(tp++) = Mz(0) * sina;
      Mz = E1 * A * Mz;
    }
    Mz = Essi * Eramp * Mz;
  }
  if (tp != settings.spg * settings.gps) {
    Log::Fail("Programmer error");
  }
  return dynamic;
}

T2InvPrep::T2InvPrep(Settings const &s)
  : Sequence{s}
{
}

auto T2InvPrep::length() const -> Index { return settings.spg * settings.gps; }

auto T2InvPrep::parameters(Index const nsamp, std::vector<float> lo, std::vector<float> hi) const -> Eigen::ArrayXXf
{
  return Parameters::T1T2B1(nsamp, lo, hi);
}

auto T2InvPrep::simulate(Eigen::ArrayXf const &p) const -> Eigen::ArrayXf
{
  float const R1 = 1.f / p(0);
  float const R2 = 1.f / p(1);
  float const B1 = p(2);
  Eigen::ArrayXf dynamic(settings.spg * settings.gps);

  Eigen::Matrix2f E1, E2, Eramp, Essi, Erec;
  float const e1 = exp(-R1 * settings.TR);
  float const eramp = exp(-R1 * settings.Tramp);
  float const essi = exp(-R1 * settings.Tssi);
  float const erec = exp(-R1 * settings.Trec);
  E1 << e1, 1 - e1, 0.f, 1.f;
  E2 << -exp(-R2 * settings.TE), 0.f, 0.f, 1.f;
  Eramp << eramp, 1 - eramp, 0.f, 1.f;
  Essi << essi, 1 - essi, 0.f, 1.f;
  Erec << erec, 1 - erec, 0.f, 1.f;

  float const cosa = cos(B1 * settings.alpha * M_PI / 180.f);
  float const sina = sin(B1 * settings.alpha * M_PI / 180.f);

  Eigen::Matrix2f A;
  A << cosa, 0.f, 0.f, 1.f;

  // Get steady state after prep-pulse for first segment
  Eigen::Matrix2f const seg = (Essi * Eramp * (E1 * A).pow(settings.spg) * Eramp).pow(settings.gps);
  Eigen::Matrix2f const SS = Essi * E2 * Erec * seg;
  float const m_ss = SS(0, 1) / (1.f - SS(0, 0));

  // Now fill in dynamic
  Index tp = 0;
  Eigen::Vector2f Mz{m_ss, 1.f};
  for (Index ig = 0; ig < settings.gps; ig++) {
    Mz = Eramp * Mz;
    for (Index ii = 0; ii < settings.spg; ii++) {
      dynamic(tp++) = Mz(0) * sina;
      Mz = E1 * A * Mz;
    }
    Mz = Essi * Eramp * Mz;
  }
  if (tp != settings.spg * settings.gps) {
    Log::Fail("Programmer error");
  }
  return dynamic;
}

} // namespace rl
