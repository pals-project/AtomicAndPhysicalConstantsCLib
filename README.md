# AtomicAndPhysicalConstantsCLib

A C/C++ library for physical constants and particle properties (mass, charge,
spin, g-factor, magnetic moment) for use in accelerator/simulation codes such as
PALS and pals-cpp.

It is a faithful C++ mirror of
[AtomicAndPhysicalConstants.jl](https://github.com/bmad-sim/AtomicAndPhysicalConstants.jl)
(AAPC), the source of truth for these numbers in the Bmad / SciBmad / PALS
ecosystem. Values are kept identical to the Julia package by **generating** the
data tables directly from it rather than transcribing them by hand.

## Layout

```
codegen/
  generate.jl                     AAPC-codegen: reads AAPC, emits the two headers below
  Project.toml                    Julia environment for the codegen
include/aapc/
  aapc.h                          Public API (hand-written): Kind, Species, accessors
  constants.generated.h           GENERATED: scalar physical constants
  species_data.generated.h        GENERATED: subatomic + atomic/isotope tables
src/
  aapc.cpp                        Logic (hand-written): Species name parser + accessors
tests/
  test_aapc.cpp                   Checks against reference values from AAPC
```

## The data / logic split

Only the **data** is generated. `codegen/generate.jl` loads the live AAPC module
and emits its scalar constants and its subatomic/atomic species tables as
`constexpr double`s and `std::unordered_map`s. Because Julia's shortest-round-trip
float formatting and C++'s `strtod` obey the same IEEE rounding, every literal
parses back to the identical bits.

The **logic** — the `Species(name)` parser (isotopes, ion charge states,
anti-particles, named nuclei) and the accessor functions (`massof`, `chargeof`,
`gyromagnetic_anomaly`, …) — is hand-written in `src/aapc.cpp`. It mirrors AAPC's
`constructors.jl` and `functions.jl`. That code is a stable algorithm rather than
CODATA-versioned data, so it does not belong in codegen.

This means a new CODATA release (or any data change in AAPC) is absorbed by
**re-running the codegen** — no hand-editing of numbers, no transcription errors.

## Regenerating the data (AAPC-codegen)

One-time environment setup (points the codegen at your AAPC checkout; use the
registered package instead if you are not tracking a local copy):

```sh
julia --project=codegen -e 'using Pkg; Pkg.develop(path="../AtomicAndPhysicalConstants"); Pkg.instantiate()'
```

Then regenerate `include/aapc/*.generated.h`:

```sh
julia --project=codegen codegen/generate.jl
```

Optionally pass an output directory: `julia --project=codegen codegen/generate.jl path/to/include/aapc`.
Commit the regenerated headers alongside a note of which AAPC version they mirror
(the version and CODATA year are stamped into each generated file's banner).

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

This builds `libaapc.a` (static) and `libaapc.dylib`/`.so` (shared, for FFI
consumers such as Julia) plus the test executable.

## Usage

```cpp
#include "aapc/aapc.h"

double m = aapc::mass_of("12C");                 // eV/c^2
double q = aapc::charge_of("H+");                // units of e
double a = aapc::anomalous_moment_of("proton");  // (g - 2) / 2

aapc::Species he = aapc::species("helion");
double spin = he.spin;                           // in units of hbar
double c    = aapc::C_LIGHT;                      // a mirrored constant
```

The three name→value helpers `mass_of`, `charge_of`, and `anomalous_moment_of`
correspond to the PALS-standard expression functions of the same names.

## Known upstream issue

`Species("triton")` currently throws, faithfully mirroring AAPC: its triton
g-factor calculation references `M_TRITON`, which AAPC's `constants.jl` does not
define. Once AAPC adds `M_TRITON`, regenerate the constants and update the one
guarded branch in `src/aapc.cpp` (it points to the exact line).
