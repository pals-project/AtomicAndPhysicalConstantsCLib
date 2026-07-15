// Tests for AtomicAndPhysicalConstantsCLib.
//
// Reference values are taken directly from AtomicAndPhysicalConstants.jl
// (CODATA 2022, v0.9.0) so a mismatch means the C++ mirror has drifted from the
// Julia source of truth. No external test framework — a tiny CHECK harness.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "aapc/aapc.h"

static int g_failures = 0;
static int g_checks = 0;

static void check_close(const char* what, double got, double want) {
  ++g_checks;
  const double tol = 1e-9 * std::max(1.0, std::fabs(want));
  if (std::fabs(got - want) > tol) {
    std::printf("FAIL %s: got %.17g want %.17g\n", what, got, want);
    ++g_failures;
  }
}

static void check_true(const char* what, bool cond) {
  ++g_checks;
  if (!cond) {
    std::printf("FAIL %s\n", what);
    ++g_failures;
  }
}

static bool throws(const std::string& name) {
  try {
    aapc::species(name);
    return false;
  } catch (const std::invalid_argument&) {
    return true;
  }
}

int main() {
  using namespace aapc;

  // --- Constants mirrored from AAPC ---
  check_close("RELEASE_YEAR", RELEASE_YEAR, 2022);
  check_close("C_LIGHT", C_LIGHT, 2.99792458e8);
  check_close("M_ELECTRON", M_ELECTRON, 510998.95069000003);
  check_close("EV_PER_AMU", EV_PER_AMU, 9.3149410242e8);

  // --- Subatomic particles ---
  check_close("electron mass", mass_of("electron"), 510998.95069000003);
  check_close("electron charge", charge_of("electron"), -1.0);
  check_close("electron ganom", anomalous_moment_of("electron"),
              0.0011596521804599913);

  check_close("proton mass", mass_of("proton"), 938272089.43000007);
  check_close("proton charge", charge_of("proton"), 1.0);
  check_close("proton ganom", anomalous_moment_of("proton"),
              1.7928473446500002);

  // Anti-particle: charge flips, mass/anomaly preserved.
  check_close("anti-proton mass", mass_of("anti-proton"), 938272089.43000007);
  check_close("anti-proton charge", charge_of("anti-proton"), -1.0);
  check_close("anti-proton ganom", anomalous_moment_of("anti-proton"),
              1.7928473446500002);

  // --- Atoms, isotopes, ions ---
  check_close("H mass", mass_of("H"), 938890867.99172413);
  check_close("H charge", charge_of("H"), 0.0);

  check_close("3He mass", mass_of("3He"), 2809413524.398952);
  check_close("12C mass", mass_of("12C"), 11177929229.039999);

  check_close("H+ mass", mass_of("H+"), 938379869.0410341);
  check_close("H+ charge", charge_of("H+"), 1.0);

  check_close("235U++ mass", mass_of("235U++"), 218941012699.86734);
  check_close("235U++ charge", charge_of("235U++"), 2.0);

  // --- Named nuclei ---
  check_close("deuteron mass", mass_of("deuteron"), 1875612929.0417254);
  check_close("deuteron charge", charge_of("deuteron"), 1.0);
  check_close("deuteron ganom", anomalous_moment_of("deuteron"),
              -0.14298726968020159);

  check_close("helion mass", mass_of("helion"), 2808391526.4975719);
  check_close("helion charge", charge_of("helion"), 2.0);
  check_close("helion ganom", anomalous_moment_of("helion"),
              -4.1841537498328929);

  // --- Species struct fields & accessors ---
  Species c12 = species("12C");
  check_close("12C spin", c12.spin, 6.0);
  check_close("12C iso", atomicnumberof(c12), 12.0);
  check_true("12C kind ATOM", kindof(c12) == Kind::ATOM);
  check_true("12C nameof", nameof(c12) == "#12C");
  check_true("235U++ nameof", nameof(species("235U++")) == "#235U+2");

  // --- Null species ---
  check_true("null isnull", isnullspecies(species("")));
  check_true("null-name isnull", isnullspecies(species("Null")));

  // --- Error cases (mirror AAPC) ---
  check_true("triton throws (M_TRITON undefined upstream)", throws("triton"));
  check_true("unknown throws", throws("nonsense"));
  check_true("bad charge throws", throws("H+-"));

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
