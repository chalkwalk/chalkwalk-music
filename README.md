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
| `Scale.h` | The tonal core: keys as a fifths window, modes as a signed brightness axis, per-degree modifiers, pitch-class masks, note strength, quantising and naming |
| `MetricGrid.h` | Metric weight from a time signature, by Lerdahl-Jackendoff hierarchy: a position's weight is the number of levels at which it heads a group. 6/8 is correctly two dotted beats rather than three |
| `AccentVel.h` | Metric weight to velocity, as a curve with a centre and a depth |
| `Density.h` | Subtractive thinning: an overlay that can only silence events, never add them |
| `MetricSelect.h` | Choosing *which* events survive a density cut -- strongest metric positions first, Euclidean-spread within a tier when the budget runs out mid-tier |
| `Tuning.h` | Scala `.scl` scales and `.kbm` keyboard mappings, as per-note deviations from 12-TET |
| `NoteStrength.h` | How good a note is here and now: against the scale, and against the chord sounding under it |

### The tuning parser takes text, not a filename

Deliberately. A parser that opens files cannot be tested without a filesystem,
cannot read a scale out of a zip, a resource bundle, an HTTP response or a
string literal, and drags a file API into a library that otherwise needs
nothing. Reading the bytes is the caller's job and it is one line.

Two quirks of the format it gets right, and which are easy to get wrong:

- **The last entry of a `.scl` file is the period, not a note.** Usually 2/1,
  but Bohlen-Pierce repeats at 3/1 and stretched piano tunings repeat slightly
  wide. Hardcoding 1200 cents is wrong outside the first period.
- **Keys below the mapping's middle note need floor division**, not C++'s
  truncation, or everything in the period below middle C lands in the wrong
  one.

### The scale model in one paragraph

A key is a contiguous **window of fifths** with the root at offset 0.
`brightness` is the window's position, which *is* the mode -- Lydian brightest
through Locrian darkest, one accidental per step, so the circle of fifths falls
out of the representation instead of being a table. Window *size* gives the
pentatonic and triad cores for free. **Modifiers** then alter individual
degrees (harmonic minor, the blue note, Neapolitan), each anchored to the
window's flat edge so it means the same thing in every relative mode. Everything
collapses to a twelve-bit pitch-class mask.

A tonic plus one of seven named modes -- the usual representation -- is a strict
special case. The cost of the general one, stated plainly: a mask has
`popcount(mask)` degrees, not always seven, so code that assumes seven has to
become "the nth set bit".

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

## Roadmap

[](ROADMAP.md). The headline gap: dissonance is
register-dependent and this model is register-blind -- it ranks pitch classes,
so / and / look identical to it, and they do not sound
identical.

## Licence

MIT. The plugins this was extracted from are GPLv3, because JUCE's free licence
is AGPLv3; this library has no such constraint and is meant to be usable
anywhere.
