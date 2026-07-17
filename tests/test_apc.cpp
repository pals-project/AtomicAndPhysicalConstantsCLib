// Tests for AtomicAndPhysicalConstantsCLib.
//
// Reference values are taken directly from AtomicAndPhysicalConstants.jl
// (CODATA 2022, APC v0.11) so a mismatch means the C++ mirror has drifted from
// the Julia source of truth. No external test framework — a tiny CHECK harness.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "apc/apc.h"

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

static void check_nan(const char* what, double got) {
  ++g_checks;
  if (!std::isnan(got)) {
    std::printf("FAIL %s: got %.17g want NaN\n", what, got);
    ++g_failures;
  }
}

static bool throws(const std::string& name) {
  try {
    apc::species(name);
    return false;
  } catch (const std::invalid_argument&) {
    return true;
  }
}

static bool throws_z(const apc::Species& s) {
  try {
    apc::atomicnumberof(s);
    return false;
  } catch (const std::invalid_argument&) {
    return true;
  }
}

int main() {
  using namespace apc;

  // --- Constants mirrored from APC ---
  check_close("RELEASE_YEAR", RELEASE_YEAR, 2022);
  check_close("C_LIGHT", C_LIGHT, 2.99792458e8);
  check_close("M_ELECTRON", M_ELECTRON, 510998.95069);
  check_close("EV_PER_AMU", EV_PER_AMU, 9.3149410372e8);

  // --- Subatomic particles ---
  check_close("electron mass", mass_of("electron"), 510998.95069);
  check_close("electron charge", charge_of("electron"), -1.0);
  check_close("electron ganom", anomalous_moment_of("electron"),
              0.0011596521804599913);

  check_close("proton mass", mass_of("proton"), 9.382720894300001e8);
  check_close("proton charge", charge_of("proton"), 1.0);
  check_close("proton ganom", anomalous_moment_of("proton"),
              1.7928473446500002);

  // Anti-particles are their own rows in the table rather than a sign flip
  // applied to the matter particle: charge is negated, mass and anomaly are not.
  check_close("anti-proton mass", mass_of("anti-proton"), 9.382720894300001e8);
  check_close("anti-proton charge", charge_of("anti-proton"), -1.0);
  check_close("anti-proton ganom", anomalous_moment_of("anti-proton"),
              1.7928473446500002);

  // --- Named nuclei: subatomic species with their own measured constants ---
  // Not atoms. `helion` is the bare 3He nucleus, so its mass is that of the
  // neutral #3He atom less its two electrons.
  check_close("deuteron mass", mass_of("deuteron"), 1.875612945e9);
  check_close("deuteron charge", charge_of("deuteron"), 1.0);
  check_close("deuteron ganom", anomalous_moment_of("deuteron"),
              -0.57128088325);

  check_close("triton mass", mass_of("triton"), 2.80892113668e9);
  check_close("triton charge", charge_of("triton"), 1.0);
  check_close("triton ganom", anomalous_moment_of("triton"), 1.978962465);

  check_close("helion mass", mass_of("helion"), 2.80839161112e9);
  check_close("helion charge", charge_of("helion"), 2.0);
  check_close("helion ganom", anomalous_moment_of("helion"),
              1.1276253497500002);
  check_close("anti-helion charge", charge_of("anti-helion"), -2.0);

  // --- Atoms, isotopes, ions ---
  check_close("H mass", mass_of("H"), 9.388908693020471e8);
  check_close("H charge", charge_of("H"), 0.0);

  check_close("#3He mass", mass_of("#3He"), 2.8094135283197904e9);
  check_close("#12C mass", mass_of("#12C"), 1.117792924464e10);

  check_close("H+ mass", mass_of("H+"), 9.383798703513571e8);
  check_close("H+ charge", charge_of("H+"), 1.0);

  check_close("#235U++ mass", mass_of("#235U++"), 2.1894101300542447e11);
  check_close("#235U++ charge", charge_of("#235U++"), 2.0);

  // --- g-factor: stored signed, reported absolute unless asked otherwise ---
  Species e = species("electron");
  check_close("electron gspin", g_spin(e), 2.00231930436092);
  check_close("electron gspin signed", g_spin(e, true), -2.00231930436092);

  // --- The anomaly is defined only for leptons and hadrons ---
  check_nan("H ganom is NaN", anomalous_moment_of("H"));
  check_nan("#12C ganom is NaN", anomalous_moment_of("#12C"));
  check_nan("photon ganom is NaN", anomalous_moment_of("photon"));
  check_nan("null ganom is NaN", gyromagnetic_anomaly(species("")));

  // --- Species struct fields & accessors ---
  Species c12 = species("#12C");
  check_close("#12C spin", c12.spin, 6.0);
  check_true("#12C kind ATOM", kindof(c12) == Kind::ATOM);

  // --- atomicnumberof is Z, iso_of is the mass number ---
  // Carbon-12 separates the two: Z = 6, A = 12. Values from APC v0.11's
  // docstring examples.
  check_close("#12C Z", atomicnumberof(c12), 6.0);
  check_close("#12C iso", iso_of(c12), 12.0);
  check_close("Fe Z", atomicnumberof(species("Fe")), 26.0);
  check_close("H+ Z", atomicnumberof(species("H+")), 1.0);
  check_close("anti-H Z", atomicnumberof(species("anti-H")), -1.0);
  // A bare symbol is the natural-abundance average, which APC marks iso == -1.
  check_close("H iso (natural abundance)", iso_of(species("H")), -1.0);
  // Non-atoms have no atomic number at all; APC errors rather than return 0.
  check_true("electron Z throws", throws_z(species("electron")));
  check_true("photon Z throws", throws_z(species("photon")));
  check_true("null Z throws", throws_z(species("")));
  check_close("electron iso is 0", iso_of(species("electron")), 0.0);
  check_true("#12C nameof", nameof(c12) == "#12C");
  check_true("#235U++ nameof", nameof(species("#235U++")) == "#235U+2");
  check_true("photon kind", kindof(species("photon")) == Kind::PHOTON);
  check_true("electron kind LEPTON", kindof(e) == Kind::LEPTON);
  check_true("triton kind HADRON", kindof(species("triton")) == Kind::HADRON);

  // --- Null species ---
  check_true("null isnull", isnullspecies(species("")));
  check_true("null-name isnull", isnullspecies(species("Null")));

  // --- Error cases (mirror APC) ---
  check_true("unknown throws", throws("nonsense"));
  check_true("bad charge throws", throws("H+-"));

  // A mass number is only accepted with a leading '#'. APC made the '#'
  // mandatory so that a bare leading digit can never be read as an isotope.
  check_true("bare 3He throws", throws("3He"));
  check_true("bare 12C throws", throws("12C"));
  check_close("#3He is the accepted form", charge_of("#3He"), 0.0);

  // Charge must be a leading sign: "+", "++", "+++" or an explicitly signed
  // integer. A trailing sign or a run of four is not accepted.
  check_close("H+1 == H+", charge_of("H+1"), charge_of("H+"));
  check_close("He+2 == He++", charge_of("He+2"), charge_of("He++"));
  check_true("trailing-sign He2+ throws", throws("He2+"));
  check_true("H++++ throws", throws("H++++"));

  // An atom cannot shed more electrons than it has; an anti-atom is bounded
  // on the negative side instead.
  check_close("Fe+26 fully stripped", charge_of("Fe+26"), 26.0);
  check_true("Fe+27 throws", throws("Fe+27"));
  check_close("anti-H- charge", charge_of("anti-H-"), -1.0);

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
