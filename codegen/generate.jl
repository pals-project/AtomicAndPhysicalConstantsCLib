#!/usr/bin/env julia
#
# APC-codegen
# ============
# Emit C++ source that mirrors the *data* of AtomicAndPhysicalConstants.jl
# (https://github.com/bmad-sim/AtomicAndPhysicalConstants.jl).
#
# AtomicAndPhysicalConstants.jl is the single source of truth for physical
# constants and particle/isotope data in the Bmad / SciBmad / PALS ecosystem.
# This script reads its live tables and writes two generated headers:
#
#   include/apc/constants.generated.h      - scalar physical constants
#   include/apc/species_data.generated.h   - subatomic + atomic species tables
#
# Only the *data* is generated. The *logic* that mirrors APC's Julia code
# (the Species name parser from constructors.jl and the accessor functions from
# functions.jl) is hand-written C++ in src/apc.cpp, because that logic is a
# stable algorithm rather than CODATA-versioned data. Re-run this script
# whenever AtomicAndPhysicalConstants.jl changes (e.g. a new CODATA release) to
# keep the C++ numbers byte-for-byte in step with the Julia package.
#
# Usage:
#   julia --project=codegen codegen/generate.jl [OUTPUT_INCLUDE_DIR]
#
# OUTPUT_INCLUDE_DIR defaults to <repo>/include/apc.

using AtomicAndPhysicalConstants
using Printf

const A = AtomicAndPhysicalConstants

const OUT_DIR = length(ARGS) >= 1 ? ARGS[1] :
                normpath(joinpath(@__DIR__, "..", "include", "apc"))

# ── Float formatting ─────────────────────────────────────────────────────────
# Julia's `repr` gives the shortest decimal string that round-trips a Float64
# under IEEE round-to-nearest; C++'s strtod uses the same rule, so the literal
# parses back to the identical bits. Guarantee the result reads as a C++ double.
function cdouble(x::Real)
    x isa Integer && return string(x)
    s = repr(Float64(x))
    occursin(r"[.eEinInNaA]", s) || (s *= ".0")  # ensure a floating literal
    return s
end

cstr(s) = '"' * s * '"'  # species names are ASCII with no quotes/backslashes

# The C++ `apc::Kind` enumerator for a subatomic species. APC v0.11 dropped the
# `kind` field from `SubatomicSpecies`; `subatomic_particle` in constructors.jl
# now derives the classification from the name, and this mirrors that rule so
# each generated row stays self-describing. (`Kind.NULL` is emitted as
# `NULL_KIND` in C++, where `NULL` is a macro, but no table row ever needs it.)
function cppkind(name)
    name == "photon" && return "Kind::PHOTON"
    name in A.leptons && return "Kind::LEPTON"
    return "Kind::HADRON"
end

const BANNER = """
// ─────────────────────────────────────────────────────────────────────────────
// GENERATED FILE — DO NOT EDIT BY HAND.
//
// Produced by codegen/generate.jl from AtomicAndPhysicalConstants.jl
// (CODATA $(A.RELEASE_YEAR), APC v$(pkgversion(A))). Regenerate with:
//     julia --project=codegen codegen/generate.jl
// ─────────────────────────────────────────────────────────────────────────────
"""

# ── constants.generated.h ────────────────────────────────────────────────────
function emit_constants(path)
    open(path, "w") do io
        print(io, BANNER)
        println(io, "#pragma once")
        println(io)
        println(io, "namespace apc {")
        println(io)
        println(io, "// CODATA release year that these values are drawn from.")
        println(io, "constexpr int RELEASE_YEAR = $(A.RELEASE_YEAR);")
        println(io)
        println(io, "// Every scalar constant exported by AtomicAndPhysicalConstants.jl,")
        println(io, "// mirrored verbatim. Units follow the Julia package (masses eV/c^2,")
        println(io, "// moments eV/T, etc.).")
        for n in sort(names(A))
            isdefined(A, n) || continue
            n === :RELEASE_YEAR && continue  # already emitted above, as an int
            v = getproperty(A, n)
            v isa Real || continue          # skip types, functions, dicts, enums
            v isa Bool && continue
            println(io, "constexpr double $(n) = $(cdouble(v));")
        end
        println(io)
        println(io, "}  // namespace apc")
    end
    return path
end

# ── species_data.generated.h ─────────────────────────────────────────────────
function emit_species(path)
    open(path, "w") do io
        print(io, BANNER)
        println(io, "#pragma once")
        println(io)
        println(io, "#include <string>")
        println(io, "#include <unordered_map>")
        println(io)
        println(io, "#include \"apc/apc.h\"  // for apc::Kind")
        println(io)
        println(io, "namespace apc {")
        println(io, "namespace data {")
        println(io)
        println(io, "struct SubatomicData {")
        println(io, "  double charge;  // units of e")
        println(io, "  double mass;    // eV/c^2")
        println(io, "  double spin;    // hbar")
        println(io, "  double moment;  // eV/T (J/T in APC, converted there)")
        println(io, "  double gspin;   // spin g-factor, stored signed")
        println(io, "  Kind kind;")
        println(io, "};")
        println(io)
        println(io, "struct AtomicData {")
        println(io, "  int Z;")
        println(io, "  // mass number (-1 = natural-abundance average) -> mass in AMU")
        println(io, "  std::unordered_map<int, double> mass;")
        println(io, "};")
        println(io)

        # subatomic species
        println(io, "inline const std::unordered_map<std::string, SubatomicData>&")
        println(io, "subatomic_species() {")
        println(io, "  static const std::unordered_map<std::string, SubatomicData> m = {")
        for name in sort(collect(keys(A.SUBATOMIC_SPECIES)))
            pd = A.SUBATOMIC_SPECIES[name]
            @printf(io, "    {%s, {%s, %s, %s, %s, %s, %s}},\n",
                    cstr(name), cdouble(pd.charge), cdouble(pd.mass),
                    cdouble(pd.spin), cdouble(pd.moment), cdouble(pd.gspin),
                    cppkind(name))
        end
        println(io, "  };")
        println(io, "  return m;")
        println(io, "}")
        println(io)

        # atomic species
        println(io, "inline const std::unordered_map<std::string, AtomicData>&")
        println(io, "atomic_species() {")
        println(io, "  static const std::unordered_map<std::string, AtomicData> m = {")
        for sym in sort(collect(keys(A.ATOMIC_SPECIES)); by = s -> A.ATOMIC_SPECIES[s].Z)
            ad = A.ATOMIC_SPECIES[sym]
            isos = sort(collect(keys(ad.mass)))
            entries = join(("{$iso, $(cdouble(ad.mass[iso]))}" for iso in isos), ", ")
            @printf(io, "    {%s, {%d, {%s}}},\n", cstr(sym), ad.Z, entries)
        end
        println(io, "  };")
        println(io, "  return m;")
        println(io, "}")
        println(io)
        println(io, "}  // namespace data")
        println(io, "}  // namespace apc")
    end
    return path
end

function main()
    mkpath(OUT_DIR)
    c = emit_constants(joinpath(OUT_DIR, "constants.generated.h"))
    s = emit_species(joinpath(OUT_DIR, "species_data.generated.h"))
    println("APC-codegen: wrote")
    println("  ", c)
    println("  ", s)
    println("Mirrored AtomicAndPhysicalConstants v$(pkgversion(A)) (CODATA $(A.RELEASE_YEAR)): ",
            length(A.SUBATOMIC_SPECIES), " subatomic + ",
            length(A.ATOMIC_SPECIES), " atomic species.")
end

main()
