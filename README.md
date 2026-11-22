# AtomicAndPhysicalConstantsCLib

A C/C++ library for physical constants and particle properties (mass, charge,
spin, g-factor, magnetic moment) for use with C/C++ code.

It is a faithful C++ mirror of
[AtomicAndPhysicalConstants.jl](https://github.com/bmad-sim/AtomicAndPhysicalConstants.jl)
(APC).

## Versioning

The version mirrors the APC release it is generated from. 
Regenerating against a new APC release means
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

## What is generated and what is hand-written

- **The numbers are generated.** `codegen/generate.jl` reads the constants and
  the particle and isotope tables straight from APC and writes them to
  `include/apc/*.generated.h`. Every value matches APC's bit for bit.
- **The code is hand-written.** `src/apc.cpp` turns a name such as `"#12C"`,
  `"H+"` or `"anti-#3He"` into a `Species`, and provides the accessors
  (`massof`, `chargeof`, …). It follows APC's `constructors.jl` and
  `functions.jl`.

So when APC's data changes, for example with a new CODATA release, just re-run
the codegen. Only a change to APC's code needs a matching edit to `apc.cpp`.

One exception: a particle's `kind` (photon, lepton or hadron). APC works it out
from the particle's name, in `subatomic_particle`. `generate.jl` applies the same
rule and stores the result in the table, so if APC changes that rule, update
`generate.jl`.

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

`anomalous_moment_of` is the charge-normalized anomaly `a = (g/q - 2)/2`, the
one in the Thomas–BMT equation: the electron and positron both get
`+0.00115965…`, and the doubly charged helion gets `-4.18415…`. It is defined
only for charged leptons and hadrons; for neutral particles (neutron,
anti-neutron, `pion0`), photons, atoms and the null species it returns NaN, as
APC does. The g-factors are normalized to each particle's own mass, so
`G_DEUTERON`, `G_HELION`, `G_TRITON` and `G_NEUTRON` differ from the NIST
values, which are quoted against the nuclear magneton.

An atom's spin is its nuclear ground-state spin from NUBASE2020 (`"#12C"` is 0,
`"#3He"` is 1/2), or NaN where NUBASE assigns none. A bare symbol such as `"He"` is the abundance average, which
has no single spin, so `spin` is NaN. An element with no stable isotopes (Tc, Pm,
and Po onward apart from Th, Pa and U) has no standard atomic weight, so it
needs a mass number: `"#209Po"` is accepted, `"Po"` throws.

## Changes from v0.11.1

Downstream code moving from the v0.11.1 tag sees these differences, all of them
from APC v0.12.0:

- `CLASSICAL_RADIUS_FACTOR` is `R_ELECTRON * M_ELECTRON` in eV·m. It was 1e6
  too small before.
- `G_DEUTERON`, `G_HELION`, `G_TRITON` and `G_NEUTRON` are renormalized to the
  particle's own mass, and `gyromagnetic_anomaly` / `anomalous_moment_of` use
  the charge-normalized formula above. The deuteron, triton and helion
  anomalies change, and neutral particles now give NaN.
- The triton's spin is 1/2 (it was 3/2). Atomic spins come from NUBASE2020
  rather than `iso / 2`.
- Bare symbols of elements with no stable isotopes (`"Po"`, `"Tc"`, …) throw.
- Only a leading `"anti-"` marks an anti-atom. `"Anti-H"` and `"antiH"` throw.
- Only `""`, `"null"`, `"Null"` and `"NULL"` give the null species.
- Error messages match APC's, and `nameof` writes an anti-isotope as
  `"anti-#3He"`.
- `spinof` and `momentof` are new.
- The CODATA 2022 constants derived in APC (`EV_PER_J`, `H_BAR`, `MU_*`, …)
  pick up APC's recomputed values. They changed only in the last few digits.

## Known limitation

APC accepts a mass number written with Unicode superscripts (`"⁴He"`, equivalent
to `"#4He"`). This mirror does not, and rejects it. ASCII forms behave
identically to APC, including the rule that a mass number is only recognised
with a leading `#` — `"#3He"` is accepted, `"3He"` is not.

## License

AtomicAndPhysicalConstantsCLib is licensed under the GNU General Public
License, version 2 only (`GPL-2.0-only`). See [LICENSE](LICENSE).
