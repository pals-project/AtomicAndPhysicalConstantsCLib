/**
 * @file apc.cpp
 * @brief Species construction and accessors — a hand port of APC's
 *        constructors.jl and functions.jl.
 *
 * The physical data consumed here is generated from
 * AtomicAndPhysicalConstants.jl by codegen/generate.jl; only the algorithm is
 * hand-written, so it stays fixed across CODATA releases.
 */

#include "apc/apc.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <regex>
#include <stdexcept>
#include <string>

#include "apc/species_data.generated.h"

namespace apc {

namespace {

// Strip a leading "anti-" prefix, mirroring APC's
// `startswith(name, "anti-")` / `chopprefix(name, "anti-")`. Only that exact,
// leading form marks an anti-atom. Returns whether it was present.
bool strip_anti(std::string& name) {
  static const std::string prefix = "anti-";
  if (name.compare(0, prefix.size(), prefix) != 0) return false;
  name.erase(0, prefix.size());
  return true;
}

// Mirrors chargeparse: "" -> 0; a run of up to three '+' or '-' -> +/-length;
// an explicitly signed integer ("+2", "-12") -> its value. Anything else is an
// error — note that a trailing-sign form such as "2+" is not accepted.
int chargeparse(const std::string& c) {
  if (c.empty()) return 0;

  static const std::regex plusses(R"(^\++$)");
  static const std::regex minuses(R"(^-+$)");
  static const std::regex signed_int(R"(^[+-]\d+$)");

  const bool all_plus = std::regex_match(c, plusses);
  if (all_plus || std::regex_match(c, minuses)) {
    if (c.size() > 3)
      throw std::invalid_argument("The given charge " + c +
                                  " is not formatted correctly.");
    return all_plus ? static_cast<int>(c.size()) : -static_cast<int>(c.size());
  }
  if (std::regex_match(c, signed_int)) return std::stoi(c);

  throw std::invalid_argument("The given charge " + c +
                              " is not formatted correctly.");
}

// Mirrors the atomic branch of `Species(name)` followed by `atomic_particle`.
// The pattern is anchored and the '#' before a mass number is mandatory, so
// "#3He" is accepted and "3He" is not. Error messages are APC's.
Species parse_atomic_species(const std::string& species_name) {
  std::string name = species_name;
  const bool anti_atom = strip_anti(name);

  static const std::regex rgas(R"(^(?:#(\d+))?([A-Z][a-z]?)([+-].*)?$)");
  const auto& atoms = data::atomic_species();

  std::smatch m;
  const bool matched = std::regex_match(name, m, rgas);
  auto it = matched ? atoms.find(m[2].str()) : atoms.end();
  if (it == atoms.end())
    throw std::invalid_argument("\"" + species_name +
                                "\" is not a recognized particle species");
  const std::string symbol = m[2].str();
  const data::AtomicData& atom = it->second;

  int iso = -1;
  if (m[1].matched) {
    iso = std::stoi(m[1].str());
    if (atom.mass.find(iso) == atom.mass.end())
      throw std::invalid_argument(std::to_string(iso) +
                                  " is not a valid isotope of " + symbol);
  }

  const std::string charge_str = m[3].matched ? m[3].str() : "";
  if (charge_str.find('+') != std::string::npos &&
      charge_str.find('-') != std::string::npos)
    throw std::invalid_argument(species_name +
                                " has an ambiguously defined charge value.");
  const int charge = chargeparse(charge_str);

  // An atom cannot shed more electrons than it has; for an anti-atom the same
  // bound applies to the negative side.
  if (!anti_atom ? charge > atom.Z : charge < -atom.Z)
    throw std::invalid_argument(
        "The specified element cannot have the specified charge.");

  // An element with no stable isotopes has no standard atomic weight, and its
  // abundance-average entry is a 0.0 placeholder; a mass number is required.
  const double amu_mass = atom.mass.at(iso);
  if (iso == -1 && amu_mass == 0.0) {
    int lo = std::numeric_limits<int>::max(), hi = 0;
    for (const auto& kv : atom.mass) {
      if (kv.first > 0) lo = std::min(lo, kv.first);
      hi = std::max(hi, kv.first);
    }
    throw std::invalid_argument(
        symbol +
        " has no standard atomic weight (it has no stable isotopes); give a "
        "mass number A as \"#A" +
        symbol + "\", with A from " + std::to_string(lo) + " to " +
        std::to_string(hi));
  }

  const double nmass = amu_mass * EV_PER_AMU;
  // Removing electrons for a positive ion lowers the mass; the anti-atom made
  // of positrons gains mass for the same nominal charge sign.
  const double mass =
      anti_atom ? nmass + M_ELECTRON * charge : nmass - M_ELECTRON * charge;

  // Nuclear spin is the tabulated NUBASE2020 ground-state value, the same for
  // an anti-nucleus by CPT. The abundance average (iso == -1) runs over
  // isotopes of differing spin, so it has none; nor do the few isotopes NUBASE
  // leaves unassigned. Both are NaN, as in APC.
  double spin = std::numeric_limits<double>::quiet_NaN();
  if (iso != -1) {
    auto sit = atom.spin.find(iso);
    if (sit != atom.spin.end()) spin = sit->second;
  }

  // Atoms carry neither a moment nor a g-factor: APC stores none for them, and
  // the accessors report 0 rather than a computed value.
  return Species{anti_atom ? "anti-" + symbol : symbol,
                 static_cast<double>(charge),
                 mass,
                 spin,
                 0.0,
                 0.0,
                 static_cast<double>(iso),
                 Kind::ATOM};
}

}  // namespace

Species species(const std::string& name) {
  // APC's `nulls`: exactly these spellings, not any casing of "null".
  if (name.empty() || name == "NULL" || name == "Null" || name == "null")
    return Species{"Null", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, Kind::NULL_KIND};

  // The name is looked up whole, before any anti- prefix is stripped: the table
  // carries explicit "anti-*" rows with their own charge and moment already
  // signed, so stripping first would find the matter particle and get the
  // charge backwards.
  const auto& sub = data::subatomic_species();
  auto it = sub.find(name);
  if (it != sub.end()) {
    const data::SubatomicData& pd = it->second;
    return Species{name,      pd.charge, pd.mass, pd.spin,
                   pd.moment, pd.gspin,  0.0,     pd.kind};
  }

  return parse_atomic_species(name);
}

double massof(const Species& s) { return s.mass; }
double chargeof(const Species& s) { return s.charge; }
double spinof(const Species& s) { return s.spin; }

double momentof(const Species& s) {
  return s.kind == Kind::ATOM || s.kind == Kind::NULL_KIND ? 0.0 : s.moment;
}

// Z is not carried on the Species — APC looks it up from the element symbol on
// demand, so this does the same. The stored name is the bare symbol ("C" for
// "#12C", "H" for "H+") with an "anti-" prefix for anti-atoms.
int atomicnumberof(const Species& s) {
  if (s.kind != Kind::ATOM)
    throw std::invalid_argument(
        "Particle species which are not atoms do not have atomic numbers.");

  std::string symbol = s.name;
  const bool anti_atom = strip_anti(symbol);

  const auto& atoms = data::atomic_species();
  auto it = atoms.find(symbol);
  if (it == atoms.end())
    throw std::invalid_argument("Element " + symbol +
                                " not found in atomic species database");
  return anti_atom ? -it->second.Z : it->second.Z;
}

double iso_of(const Species& s) { return s.kind == Kind::ATOM ? s.iso : 0.0; }

Kind kindof(const Species& s) { return s.kind; }
bool isnullspecies(const Species& s) { return s.kind == Kind::NULL_KIND; }

double g_spin(const Species& s, bool is_signed) {
  return is_signed ? s.gspin : std::fabs(s.gspin);
}

// a = (g_q - 2) / 2 with g_q = g / q, the g-factor normalized to the particle's
// own charge. Dividing the signed g by q fixes the sign (electron and positron
// agree) and handles the doubly charged helion. Undefined for neutral particles.
double gyromagnetic_anomaly(const Species& s) {
  const double q = s.charge;
  if ((s.kind != Kind::LEPTON && s.kind != Kind::HADRON) || q == 0.0)
    return std::numeric_limits<double>::quiet_NaN();
  const double g_q = s.gspin / q;
  return (g_q - 2.0) / 2.0;
}

std::string nameof(const Species& s, bool basename) {
  if (s.kind == Kind::NULL_KIND) return "Null";
  if (!basename && s.kind == Kind::ATOM) {
    const int iso = static_cast<int>(s.iso);
    const int ch = static_cast<int>(s.charge);
    std::string out = s.name;
    // The mass-number prefix belongs on the symbol: "anti-#3He", not
    // "#3anti-He".
    if (iso != -1) {
      std::string symbol = s.name;
      const bool anti_atom = strip_anti(symbol);
      out = (anti_atom ? "anti-#" : "#") + std::to_string(iso) + symbol;
    }
    if (ch > 0)
      out += "+" + std::to_string(ch);
    else if (ch < 0)
      out += std::to_string(ch);
    return out;
  }
  return s.name;
}

double mass_of(const std::string& name) { return massof(species(name)); }
double charge_of(const std::string& name) { return chargeof(species(name)); }
double anomalous_moment_of(const std::string& name) {
  return gyromagnetic_anomaly(species(name));
}

}  // namespace apc
