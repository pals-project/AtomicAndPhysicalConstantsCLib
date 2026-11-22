# Maintaining AtomicAndPhysicalConstantsCLib

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

## Regenerating against a new APC release

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

Finally, bump `project(... VERSION)` in `CMakeLists.txt` to the APC version and
tag the commit to match.
