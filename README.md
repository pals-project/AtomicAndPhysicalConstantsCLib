# AtomicAndPhysicalConstantsCLib

A C/C++ library for physical constants and particle properties (mass, charge,
spin, g-factor, magnetic moment) for use with C/C++ code.

It is a faithful C++ mirror of
[AtomicAndPhysicalConstants.jl](https://github.com/bmad-sim/AtomicAndPhysicalConstants.jl)
(APC).

## Versioning

The version mirrors the APC release it is generated from. Pin a release tag
rather than tracking `main`, so that a push here cannot change your build
without you asking for it.

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

## Known limitation

APC accepts a mass number written with Unicode superscripts (`"⁴He"`, equivalent
to `"#4He"`). This mirror does not, and rejects it. ASCII forms behave
identically to APC, including the rule that a mass number is only recognised
with a leading `#` — `"#3He"` is accepted, `"3He"` is not.

## Maintaining

See [MAINTAINING.md](MAINTAINING.md) for how the code is organized and how to
regenerate it against a new APC release.

## License

AtomicAndPhysicalConstantsCLib is licensed under the GNU General Public
License, version 2 only (`GPL-2.0-only`). See [LICENSE](LICENSE).
