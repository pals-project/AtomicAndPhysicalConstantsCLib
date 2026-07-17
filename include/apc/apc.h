#ifndef APC_APC_H
#define APC_APC_H

/**
 * @file apc.h
 * @brief C++ mirror of AtomicAndPhysicalConstants.jl.
 *
 * Physical constants and particle/isotope data come from
 * AtomicAndPhysicalConstants.jl via codegen/generate.jl (see
 * constants.generated.h). The Species name parser and accessor functions
 * declared here mirror constructors.jl and functions.jl of that package and are
 * implemented in src/apc.cpp.
 */

#include <string>

#include "apc/constants.generated.h"  // constexpr double constants + RELEASE_YEAR

namespace apc {

/// Particle classification. Mirrors APC's `@enum Kind ATOM HADRON LEPTON
/// PHOTON NULL`; `NULL` is renamed `NULL_KIND` to avoid the C `NULL` macro.
enum class Kind { ATOM, HADRON, LEPTON, PHOTON, NULL_KIND };

/// A particle or atom with its physical properties. Mirrors the Julia
/// `Species` struct (types.jl).
struct Species {
  std::string name;  ///< Name as constructed, e.g. "electron", "H", "anti-He".
  double charge;     ///< Charge in units of the elementary charge e.
  double mass;       ///< Mass in eV/c^2.
  double spin;       ///< Spin in units of hbar.
  double moment;     ///< Magnetic moment in eV/T (0 for atoms).
  double gspin;      ///< Spin g-factor, stored signed (0 for atoms).
  double iso;        ///< Mass number (0 subatomic, -1 average, else isotope).
  Kind kind;         ///< Particle classification.
};

/**
 * Constructs a Species from a name, mirroring `Species(name::String)`.
 *
 * Accepts subatomic particles ("electron", "proton", "pion+", ...),
 * anti-particles ("anti-proton"), named nuclei ("deuteron", "triton",
 * "helion"), bare elements ("H", "Fe"), isotopes ("12C", "235U") and ion
 * charge states ("H+", "He++", "3He++"). Isotope numbers precede the symbol and
 * may carry a leading '#'; charge follows the symbol.
 *
 * @param name Species name.
 * @return The constructed Species. An empty or "null" name yields the null
 *         species (kind == NULL_KIND).
 * @throws std::invalid_argument if the name is not found in the databases.
 */
Species species(const std::string& name);

// --- Accessors (mirror functions.jl) ---
double massof(const Species& s);             ///< Mass in eV/c^2.
double chargeof(const Species& s);           ///< Charge in units of e.
double atomicnumberof(const Species& s);     ///< Mass number (`iso`).
Kind kindof(const Species& s);               ///< Particle classification.
bool isnullspecies(const Species& s);        ///< True for the null species.

/// Spin g-factor. Absolute value by default; pass `is_signed` for the signed
/// value (negative for e.g. the electron). Mirrors `gspin_of(; signed)`.
double g_spin(const Species& s, bool is_signed = false);

/// Gyromagnetic anomaly (|g| - 2) / 2. Defined only for leptons and hadrons;
/// returns NaN for photons, atoms and the null species, as APC does.
double gyromagnetic_anomaly(const Species& s);

/// Name of the species; for atoms includes isotope/charge unless basename.
/// Mirrors `Base.nameof(::Species; basename)`.
std::string nameof(const Species& s, bool basename = false);

// --- PALS standard functions (name -> value convenience wrappers) ---
double mass_of(const std::string& name);              ///< massof(species(name)).
double charge_of(const std::string& name);            ///< chargeof(species(name)).
/// gyromagnetic_anomaly(species(name)); NaN for photons, atoms and null.
double anomalous_moment_of(const std::string& name);

}  // namespace apc

#endif  // APC_APC_H
