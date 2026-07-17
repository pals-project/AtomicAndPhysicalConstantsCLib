# AtomicAndPhysicalConstantsCLib

A C/C++ library for physical constants and particle properties (mass, charge,
spin, g-factor, magnetic moment) for use in accelerator/simulation codes such as
PALS and pals-cpp.

It is a faithful C++ mirror of
[AtomicAndPhysicalConstants.jl](https://github.com/bmad-sim/AtomicAndPhysicalConstants.jl)
(APC), the source of truth for these numbers in the Bmad / SciBmad / PALS
ecosystem. Values are kept identical to the Julia package by **generating** the
data tables directly from it rather than transcribing them by hand.

## Versioning

The version mirrors the APC release it is generated from: `apc` **v0.11.1** is
the C++ view of APC v0.11.1. Regenerating against a new APC release means
bumping `project(... VERSION)` in `CMakeLists.txt` and tagging the commit to
match. Downstream projects should pin that tag rather than track `main`, so that
a push here cannot change their build without them asking for it.

## Layout

```
codegen/
  generate.jl                     APC-codegen: reads APC, emits the two headers below
  Project.toml                    Julia environment for the codegen
include/apc/
  apc.h                          Public API (hand-written): Kind, Species, accessors
  constants.generated.h           GENERATED: scalar physical constants
  species_data.generated.h        GENERATED: subatomic + atomic/isotope tables
src/
  apc.cpp                        Logic (hand-written): Species name parser + accessors
tests/
  test_apc.cpp                   Checks against reference values from APC
```

## The data / logic split

Only the **data** is generated. `codegen/generate.jl` loads the live APC module
and emits its scalar constants and its subatomic/atomic species tables as
`constexpr double`s and `std::unordered_map`s. Because Julia's shortest-round-trip
float formatting and C++'s `strtod` obey the same IEEE rounding, every literal
parses back to the identical bits.

The **logic** — the `Species(name)` parser (isotopes, ion charge states,
anti-atoms) and the accessor functions (`massof`, `chargeof`,
`gyromagnetic_anomaly`, …) — is hand-written in `src/apc.cpp`. It mirrors APC's
`constructors.jl` and `functions.jl`. That code is a stable algorithm rather than
CODATA-versioned data, so it does not belong in codegen.

One deliberate exception: APC has no `kind` field on `SubatomicSpecies`, deriving
the classification by name in `subatomic_particle`. The codegen applies that same
rule and emits a `kind` per row, so each generated row is self-describing. If APC
ever changes how it classifies a particle, that rule must be updated in
`generate.jl` rather than in `apc.cpp`.

This means a new CODATA release (or any data change in APC) is absorbed by
**re-running the codegen** — no hand-editing of numbers, no transcription errors.

## Regenerating the data (APC-codegen)

One-time environment setup (points the codegen at your APC checkout; use the
registered package instead if you are not tracking a local copy):

```sh
julia --project=codegen -e 'using Pkg; Pkg.develop(path="../AtomicAndPhysicalConstants"); Pkg.instantiate()'
```

Then regenerate `include/apc/*.generated.h`:

```sh
julia --project=codegen codegen/generate.jl
```

Optionally pass an output directory: `julia --project=codegen codegen/generate.jl path/to/include/apc`.
Commit the regenerated headers alongside a note of which APC version they mirror
(the version and CODATA year are stamped into each generated file's banner).

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

This builds `libapc.a` (static) and `libapc.dylib`/`.so` (shared, for FFI
consumers such as Julia) plus the test executable.

## Usage

```cpp
#include "apc/apc.h"

double m = apc::mass_of("#12C");                // eV/c^2
double q = apc::charge_of("H+");                // units of e
double a = apc::anomalous_moment_of("proton");  // (|g| - 2) / 2

apc::Species he = apc::species("helion");
double spin = he.spin;                           // in units of hbar
double c    = apc::C_LIGHT;                      // a mirrored constant
```

The three name→value helpers `mass_of`, `charge_of`, and `anomalous_moment_of`
correspond to the PALS-standard expression functions of the same names.

`anomalous_moment_of` is defined only for leptons and hadrons; for photons,
atoms and the null species it returns NaN, as APC does.

## Known upstream issues

`Species("triton")` and `Species("anti-triton")` report `spin = 1.5`. The triton
is physically spin-1/2; 1.5 is what the atomic parser's `0.5 * iso` heuristic
yields for A=3, and the value carried over when the triton became a subatomic
species. Reported upstream in APC PR #294. The mirror reproduces it deliberately
— it reflects APC, not a porting error.

The gyromagnetic anomaly is computed from the *absolute* g-factor, so species
with a negative g-factor (helion, deuteron, neutron, …) get an anomaly that does
not correspond to the usual signed definition. Again this mirrors APC's
`gyromagnetic_anomaly`.

## Known limitation

APC accepts a mass number written with Unicode superscripts (`"⁴He"`, equivalent
to `"#4He"`). This mirror does not, and rejects it. ASCII forms behave
identically to APC, including the rule that a mass number is only recognised
with a leading `#` — `"#3He"` is accepted, `"3He"` is not.
