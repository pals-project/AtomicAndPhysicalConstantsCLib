/**
 * @file aapc.cpp
 * @brief Species construction and accessors — a hand port of AAPC's
 *        constructors.jl and functions.jl.
 *
 * The physical data consumed here is generated from
 * AtomicAndPhysicalConstants.jl by codegen/generate.jl; only the algorithm is
 * hand-written, so it stays fixed across CODATA releases.
 */

#include "aapc/aapc.h"

#include <cctype>
#include <cmath>
#include <regex>
#include <stdexcept>
#include <string>

#include "aapc/species_data.generated.h"

namespace aapc {

namespace {

// Strip any anti-particle prefix ("anti-", "anti", "Anti-", "Anti") wherever it
// appears, mirroring Julia's `ANTI_REGEX`. Returns whether one was present.
bool strip_anti(std::string& name) {
  static const std::regex anti(R"(anti-|anti|Anti-|Anti)");
  if (!std::regex_search(name, anti)) return false;
  name = std::regex_replace(name, anti, "");
  return true;
}

// Parse a bare atomic name into (symbol, iso, charge). Mirrors
// parse_atomic_name; `iso` defaults to -1 (natural abundance) and `charge` to 0.
void parse_atomic_name(const std::string& name, std::string& symbol, int& iso,
                       int& charge) {
  static const std::regex rgas(R"([A-Z][a-z]|[A-Z])");  // atomic symbol
  charge = 0;
  iso = -1;

  std::smatch m;
  if (!std::regex_search(name, m, rgas))
    throw std::invalid_argument(
        "The specified particle name does not exist in this library.");

  symbol = m.str();
  const size_t symbol_start = static_cast<size_t>(m.position(0));
  const size_t symbol_end = symbol_start + symbol.size();  // one past the symbol

  // Left of the symbol: isotope number, optionally prefixed with '#'.
  std::string left = name.substr(0, symbol_start);
  if (!left.empty()) {
    if (left.front() == '#') left.erase(left.begin());
    try {
      size_t pos = 0;
      iso = std::stoi(left, &pos);
      if (pos != left.size()) throw std::invalid_argument("");
    } catch (...) {
      throw std::invalid_argument("Invalid isotope number format: \"" + left +
                                  "\"");
    }
  }

  // Right of the symbol: charge state.
  std::string right = name.substr(symbol_end);
  if (right.empty()) return;

  const bool has_plus = right.find('+') != std::string::npos;
  const bool has_minus = right.find('-') != std::string::npos;
  auto parse_int = [&](const std::string& s) -> int {
    try {
      size_t pos = 0;
      int v = std::stoi(s, &pos);
      if (pos != s.size()) throw std::invalid_argument("");
      return v;
    } catch (...) {
      throw std::invalid_argument("Invalid charge format: \"" + right + "\"");
    }
  };

  if (has_plus && has_minus) {
    throw std::invalid_argument("You made a typo in \"" + right +
                                "\". You have both + and - in the name.");
  } else if (right.front() == '+') {
    if (right == "+")
      charge = 1;
    else if (right == "++")
      charge = 2;
    else
      charge = parse_int(right.substr(1));
  } else if (right.front() == '-') {
    if (right == "-")
      charge = -1;
    else if (right == "--")
      charge = -2;
    else
      charge = -parse_int(right.substr(1));
  } else if (right.back() == '+') {
    charge = parse_int(right.substr(0, right.size() - 1));
  } else if (right.back() == '-') {
    charge = -parse_int(right.substr(0, right.size() - 1));
  } else {
    throw std::invalid_argument("Invalid characters after atomic symbol: \"" +
                                right + "\"");
  }
}

// Mirrors parse_atomic_species: build an atomic/anti-atomic Species.
Species parse_atomic_species(const std::string& name) {
  std::string local = name;
  const bool anti_atom = strip_anti(local);

  std::string symbol;
  int iso, charge;
  if (local == "helion") {
    symbol = "He";
    iso = 3;
    charge = 2;
  } else if (local == "triton") {
    symbol = "H";
    iso = 3;
    charge = 1;
  } else if (local == "deuteron") {
    symbol = "H";
    iso = 2;
    charge = 1;
  } else {
    parse_atomic_name(local, symbol, iso, charge);
  }

  const auto& atoms = data::atomic_species();
  auto it = atoms.find(symbol);
  if (it == atoms.end())
    throw std::invalid_argument("Element " + symbol +
                                " not found in atomic species database");
  const data::AtomicData& atom = it->second;

  auto miso = atom.mass.find(iso);
  if (miso == atom.mass.end())
    throw std::invalid_argument("The isotope you specified is not available.");
  if (charge > atom.Z)
    throw std::invalid_argument(
        "You have specified a larger positive charge than the fully stripped " +
        symbol + " atom has, which is unphysical.");

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

  const auto& gmap = data::g_factor_map();
  double g_factor = 0.0;
  if (symbol == "He" && iso == 3 && charge == 2)
    g_factor = gmap.at("helion") * M_HELION / (2 * M_PROTON);
  else if (symbol == "H" && iso == 3 && charge == 1)
    // Mirrors AAPC: its triton g-factor uses M_TRITON, which constants.jl does
    // not define, so `Species("triton")` throws upstream. Reproduce that error
    // faithfully. When AAPC adds M_TRITON, emit it via codegen and replace this
    // with: g_factor = gmap.at("triton") * M_TRITON / M_PROTON;
    throw std::invalid_argument("UndefVarError: `M_TRITON` not defined");
  else if (symbol == "H" && iso == 2 && charge == 1)
    g_factor = gmap.at("deuteron") * M_DEUTERON / M_PROTON;

  return Species{anti_atom ? "anti-" + symbol : symbol,
                 static_cast<double>(charge),
                 mass,
                 spin,
                 0.0,
                 g_factor,
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

  std::string clean = name;
  const bool is_anti = strip_anti(clean);

  const auto& sub = data::subatomic_species();
  auto it = sub.find(clean);
  if (it != sub.end()) {
    const data::SubatomicData& pd = it->second;
    const auto& gmap = data::g_factor_map();
    auto git = gmap.find(clean);
    const double g_factor = git != gmap.end() ? git->second : 0.0;
    return Species{name,
                   is_anti ? -pd.charge : pd.charge,
                   pd.mass,
                   pd.spin,
                   is_anti ? -pd.moment : pd.moment,
                   g_factor,
                   0.0,
                   pd.kind};
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
double g_spin(const Species& s) { return s.g_factor; }
double gyromagnetic_anomaly(const Species& s) { return (s.g_factor - 2.0) / 2.0; }
bool isnullspecies(const Species& s) { return s.kind == Kind::NULL_KIND; }

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

}  // namespace aapc
