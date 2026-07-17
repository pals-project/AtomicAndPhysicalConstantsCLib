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

#include <cctype>
#include <cmath>
#include <limits>
#include <regex>
#include <stdexcept>
#include <string>

#include "apc/species_data.generated.h"

namespace apc {

namespace {

// Strip any anti-particle prefix ("anti-", "anti", "Anti-", "Anti") wherever it
// appears, mirroring Julia's `ANTI_REGEX`. Returns whether one was present.
bool strip_anti(std::string& name) {
  static const std::regex anti(R"(anti-|anti|Anti-|Anti)");
  if (!std::regex_search(name, anti)) return false;
  name = std::regex_replace(name, anti, "");
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

// Parse a bare atomic name into (symbol, iso, charge), mirroring the atomic
// branch of `Species(name)`. The pattern is anchored and the '#' before a mass
// number is mandatory, so "#3He" is accepted and "3He" is not.
void parse_atomic_name(const std::string& name, std::string& symbol, int& iso,
                       int& charge) {
  static const std::regex rgas(R"(^(?:#(\d+))?([A-Z][a-z]?)([+-].*)?$)");

  std::smatch m;
  if (!std::regex_match(name, m, rgas))
    throw std::invalid_argument(
        "you did not specify an atomic species or subatomic species in " +
        name);

  iso = m[1].matched ? std::stoi(m[1].str()) : -1;
  symbol = m[2].str();

  const std::string charge_str = m[3].matched ? m[3].str() : "";
  if (charge_str.find('+') != std::string::npos &&
      charge_str.find('-') != std::string::npos)
    throw std::invalid_argument(name +
                                " has an ambiguously defined charge value.");
  charge = chargeparse(charge_str);
}

// Mirrors atomic_particle: build an atomic/anti-atomic Species.
Species parse_atomic_species(const std::string& name) {
  std::string local = name;
  const bool anti_atom = strip_anti(local);

  std::string symbol;
  int iso, charge;
  parse_atomic_name(local, symbol, iso, charge);

  const auto& atoms = data::atomic_species();
  auto it = atoms.find(symbol);
  if (it == atoms.end())
    throw std::invalid_argument("Element " + symbol +
                                " not found in atomic species database");
  const data::AtomicData& atom = it->second;

  auto miso = atom.mass.find(iso);
  if (miso == atom.mass.end())
    throw std::invalid_argument("The isotope you specified is not available.");
  // An atom cannot shed more electrons than it has; for an anti-atom the same
  // bound applies to the negative side.
  if (!anti_atom ? charge > atom.Z : charge < -atom.Z)
    throw std::invalid_argument("The specified element " + symbol +
                                " cannot have the specified charge.");

  const double amu_mass = miso->second;
  const double nmass = amu_mass * EV_PER_AMU;
  // Removing electrons for a positive ion lowers the mass; the anti-atom made
  // of positrons gains mass for the same nominal charge sign.
  const double mass =
      anti_atom ? nmass + M_ELECTRON * charge : nmass - M_ELECTRON * charge;

  double spin;
  if (iso == -1) {
    const double partonum = std::round(amu_mass);
    spin = anti_atom ? 0.5 * (partonum + (atom.Z + charge))
                     : 0.5 * (partonum + (atom.Z - charge));
  } else {
    spin = 0.5 * iso;
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

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(c));
  return s;
}

}  // namespace

Species species(const std::string& name) {
  if (name.empty() || lower(name) == "null")
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

  try {
    return parse_atomic_species(name);
  } catch (const std::exception& e) {
    throw std::invalid_argument(
        "Species '" + name +
        "' not found in subatomic or atomic species database: " + e.what());
  }
}

double massof(const Species& s) { return s.mass; }
double chargeof(const Species& s) { return s.charge; }
double atomicnumberof(const Species& s) { return s.iso; }
Kind kindof(const Species& s) { return s.kind; }
bool isnullspecies(const Species& s) { return s.kind == Kind::NULL_KIND; }

double g_spin(const Species& s, bool is_signed) {
  return is_signed ? s.gspin : std::fabs(s.gspin);
}

double gyromagnetic_anomaly(const Species& s) {
  if (s.kind != Kind::LEPTON && s.kind != Kind::HADRON)
    return std::numeric_limits<double>::quiet_NaN();
  return (g_spin(s) - 2.0) / 2.0;
}

std::string nameof(const Species& s, bool basename) {
  if (s.kind == Kind::NULL_KIND) return "Null";
  if (!basename && s.kind == Kind::ATOM) {
    const int iso = static_cast<int>(s.iso);
    const int ch = static_cast<int>(s.charge);
    std::string out;
    if (iso != -1) out += "#" + std::to_string(iso);
    out += s.name;
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
