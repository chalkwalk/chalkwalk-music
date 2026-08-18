# chalkwalk-music

Music theory for C++ audio software. Header-only, dependency-free, MIT.

**No framework.** No JUCE, no openFrameworks, no `std::filesystem`, nothing but
the standard library. C++17. This is deliberate: the existing C++ options are
tied to a framework (`ofxMusicTheory` needs openFrameworks), narrow (`Septima`
does seventh-chord voice leading), or a whole scoring environment (`CFugue`).

> **Status: early.** Euclidean rhythm is complete and heavily tested. The rest
> of the pitch model is moving in from the projects this was extracted from --
> see *What is here* below for the order.

## What is here

| | |
|---|---|
| `Euclidean.h` | Euclidean rhythms: patterns, an allocation-free membership test, pattern period, coprime pulse selection, and Euclidean accent placement |

Planned, in the order they are moving: the scale model (keys as pitch-class
masks with a signed brightness axis and per-degree modifiers), note strength
against a scale *and* a chord, metric grids and accents, and a Scala
`.scl`/`.kbm` tuning parser.

## Using it

Header-only. Add the include directory, or with CMake:

```cmake
# As a submodule, or via FetchContent.
add_subdirectory(external/chalkwalk-music)
target_link_libraries(your_target PRIVATE chalkwalk::music)
```

```cmake
include(FetchContent)
FetchContent_Declare(chalkwalk_music
    GIT_REPOSITORY https://github.com/chalkwalk/chalkwalk-music.git
    GIT_TAG main)
FetchContent_MakeAvailable(chalkwalk_music)
```

```cpp
#include <chalkwalk/music/Euclidean.h>

namespace m = chalkwalk::music;

const auto tresillo = m::pattern(8, 3);          // x..x..x.
const bool onsetNow = m::hit(step, 8, 3);        // no allocation; audio-safe
const int  period   = m::patternPeriod(32, 9);   // 32 -- spans the whole bar
```

## The phase contract

**Step 0 is always an onset.**

"The Euclidean pattern for E(k,n)" does not name a sequence; it names a
*necklace*, and any rotation of it has an equal claim to the name. Libraries
disagree about which rotation they return, silently, and the disagreement only
shows up when a figure lands off the downbeat.

This library anchors on step 0 and asserts it exhaustively. Every other
rotation is reachable through the `offset` argument, so a caller wanting a
different phase asks for it rather than inheriting one.

A consequence worth knowing: **E(5,8) is `x.x.xx.x`, which is a rotation of the
cinquillo (`x.xx.xx.`), not the cinquillo itself** -- use `offset = 6` for that.
E(3,8) *is* the tresillo exactly.

## Building the tests

```bash
cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

Catch2, fetched automatically. Needs no JUCE and no parent project.

## Where this came from

Three audio projects grew Euclidean generators independently, and two of them
disagreed about the rotation -- producing identical rhythms that started in
different places, which nobody noticed until the code was compared. That is the
argument for this library in one sentence: the same small pieces of music
theory get retyped, and the copies drift in ways that are invisible until they
are put side by side.

## Licence

MIT. The plugins this was extracted from are GPLv3, because JUCE's free licence
is AGPLv3; this library has no such constraint and is meant to be usable
anywhere.
