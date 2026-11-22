// Tests for AtomicAndPhysicalConstantsCLib.
//
// Reference values are taken directly from AtomicAndPhysicalConstants.jl
// (CODATA 2022, APC v0.12.0) so a mismatch means the C++ mirror has drifted from
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

// The error message species(name) throws, or "" if it does not throw.
static std::string error_of(const std::string& name) {
  try {
    apc::species(name);
    return "";
  } catch (const std::invalid_argument& e) {
    return e.what();
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
  // APC v0.12 corrected this to r_e m_e c^2 in eV*m (it was 1e6 too small).
  check_close("CLASSICAL_RADIUS_FACTOR", CLASSICAL_RADIUS_FACTOR,
              R_ELECTRON * M_ELECTRON);
  // The nuclear g-factors are renormalized from the nuclear magneton to each
  // particle's own mass, so the anomaly a = (g_q - 2) / 2 holds for them too.
  check_close("G_DEUTERON", G_DEUTERON, 1.714025460639597);
  check_close("G_HELION", G_HELION, -12.736614999331573);
  check_close("G_TRITON", G_TRITON, 17.8363413504034);
  check_close("G_NEUTRON", G_NEUTRON, -3.8313594707492573);

  // --- Subatomic particles ---
  check_close("electron mass", mass_of("electron"), 510998.95069);
  check_close("electron charge", charge_of("electron"), -1.0);
  check_close("electron ganom", anomalous_moment_of("electron"),
              0.0011596521804599913);
  // The anomaly divides the signed g by the charge, so the positron (g > 0,
  // q > 0) gets the same anomaly as the electron (g < 0, q < 0).
  check_close("positron ganom", anomalous_moment_of("positron"),
              0.0011596521804599913);
  check_close("muon ganom", anomalous_moment_of("muon"),
              0.0011659206150000934);
  check_close("anti-muon ganom", anomalous_moment_of("anti-muon"),
              0.0011659206150000934);

  check_close("proton mass", mass_of("proton"), 9.3827208943e8);
  check_close("proton charge", charge_of("proton"), 1.0);
  check_close("proton ganom", anomalous_moment_of("proton"),
              1.7928473446500002);

  // Anti-particles are their own rows in the table rather than a sign flip
  // applied to the matter particle: charge is negated, mass and anomaly are not.
  check_close("anti-proton mass", mass_of("anti-proton"), 9.3827208943e8);
  check_close("anti-proton charge", charge_of("anti-proton"), -1.0);
  check_close("anti-proton ganom", anomalous_moment_of("anti-proton"),
              1.7928473446500002);

  // --- Named nuclei: subatomic species with their own measured constants ---
  // Not atoms. `helion` is the bare 3He nucleus, so its mass is that of the
  // neutral #3He atom less its two electrons.
  check_close("deuteron mass", mass_of("deuteron"), 1.875612945e9);
  check_close("deuteron charge", charge_of("deuteron"), 1.0);
  check_close("deuteron ganom", anomalous_moment_of("deuteron"),
              -0.14298726968020148);

  check_close("triton mass", mass_of("triton"), 2.80892113668e9);
  check_close("triton charge", charge_of("triton"), 1.0);
  check_close("triton ganom", anomalous_moment_of("triton"),
              7.918170675201701);
  // APC v0.12 fixed the triton's spin to 1/2 (it was 3/2).
  check_close("triton spin", spinof(species("triton")), 0.5);
  check_close("anti-triton spin", spinof(species("anti-triton")), 0.5);

  check_close("helion mass", mass_of("helion"), 2.80839161112e9);
  check_close("helion charge", charge_of("helion"), 2.0);
  // Doubly charged: g_q = g / 2, not |g|.
  check_close("helion ganom", anomalous_moment_of("helion"),
              -4.184153749832893);
  check_close("anti-helion charge", charge_of("anti-helion"), -2.0);
  check_close("anti-helion ganom", anomalous_moment_of("anti-helion"),
              -4.184153749832893);

  // Neutral particles have no charge-normalized g-factor.
  check_nan("neutron ganom is NaN", anomalous_moment_of("neutron"));
  check_nan("anti-neutron ganom is NaN", anomalous_moment_of("anti-neutron"));
  check_nan("pion0 ganom is NaN", anomalous_moment_of("pion0"));

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

  // --- Moments: stored for subatomic particles, 0 for atoms ---
  check_close("electron moment", momentof(e), -5.795094307747843e-5);
  check_close("positron moment", momentof(species("positron")),
              5.795094307747843e-5);
  check_close("H moment", momentof(species("H")), 0.0);
  check_close("null moment", momentof(species("")), 0.0);

  // --- Nuclear spin from NUBASE2020, not from the mass number ---
  Species c12 = species("#12C");
  check_close("#12C spin (even-even)", spinof(c12), 0.0);
  check_close("#4He spin (even-even)", spinof(species("#4He")), 0.0);
  check_close("#2H spin", spinof(species("#2H")), 1.0);
  check_close("#3He spin", spinof(species("#3He")), 0.5);
  check_close("#3He+2 spin (ionisation ignored)", spinof(species("#3He+2")),
              0.5);
  check_close("anti-#3He spin (same as mirror)", spinof(species("anti-#3He")),
              0.5);
  check_close("#235U++ spin", spinof(species("#235U++")), 3.5);
  check_close("#209Po spin", spinof(species("#209Po")), 0.5);
  // No single spin for an abundance average, nor where NUBASE assigns none.
  check_nan("H spin (abundance average)", spinof(species("H")));
  check_nan("He spin (abundance average)", spinof(species("He")));
  check_nan("#10Li spin (unassigned)", spinof(species("#10Li")));
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
  check_true("Li+ nameof", nameof(species("Li+")) == "Li+1");
  check_true("anti-#3He nameof", nameof(species("anti-#3He")) == "anti-#3He");
  check_true("anti-#4He+2 nameof",
             nameof(species("anti-#4He+2")) == "anti-#4He+2");
  check_true("anti-#4He+2 basename",
             nameof(species("anti-#4He+2"), true) == "anti-He");
  check_true("photon kind", kindof(species("photon")) == Kind::PHOTON);
  check_true("electron kind LEPTON", kindof(e) == Kind::LEPTON);
  check_true("triton kind HADRON", kindof(species("triton")) == Kind::HADRON);

  // --- Null species ---
  check_true("null isnull", isnullspecies(species("")));
  check_true("null-name isnull", isnullspecies(species("Null")));
  check_true("NULL isnull", isnullspecies(species("NULL")));
  // APC lists the null spellings exactly; other casings are not species.
  check_true("nUll throws", throws("nUll"));

  // --- Error cases (mirror APC) ---
  check_true("unknown throws", throws("nonsense"));
  check_true("unknown message",
             error_of("nonsense") ==
                 "\"nonsense\" is not a recognized particle species");
  check_true("bad charge throws", throws("H+-"));
  check_true("bad isotope message",
             error_of("#300H") == "300 is not a valid isotope of H");

  // Only a leading "anti-" marks an anti-atom; APC v0.12 dropped the looser
  // "anti", "Anti-" and "Anti" spellings and no longer strips it mid-name.
  check_true("Anti-H throws", throws("Anti-H"));
  check_true("antiH throws", throws("antiH"));
  check_true("antianti-H throws", throws("antianti-H"));
  check_true("anti-electron throws", throws("anti-electron"));

  // An element with no stable isotopes has no standard atomic weight, so a
  // mass number is required. Th, Pa and U are the exceptions after Bi.
  check_true("Po throws", throws("Po"));
  check_true("Tc throws", throws("Tc"));
  check_true("Pm throws", throws("Pm"));
  check_true("anti-Po throws", throws("anti-Po"));
  check_true("Po message",
             error_of("Po") ==
                 "Po has no standard atomic weight (it has no stable "
                 "isotopes); give a mass number A as \"#APo\", with A from "
                 "186 to 227");
  check_close("#209Po mass", mass_of("#209Po"), 1.9466590207127292e11);
  check_close("#99Tc mass", mass_of("#99Tc"), 9.213058944125153e10);
  check_close("Th mass", mass_of("Th"), 2.1614208081635236e11);
  check_close("U mass", mass_of("U"), 2.217225266099297e11);

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
  check_close("anti-H- mass", mass_of("anti-H-"), 9.383798703513571e8);
  check_close("anti-#4He+2 mass", mass_of("anti-#4He+2"), 3.72942332865396e9);

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
