// SPDX-License-Identifier: MIT
// Part of chalkwalk-music. See LICENSE.
#pragma once

#include <algorithm>
#include <vector>

// Euclidean rhythms: distribute `pulses` onsets as evenly as possible over
// `steps` steps.
//
// One integer buys a pattern that is already idiomatic rather than mechanical.
// That is why these are worth having: a drum part worth playing along to, from
// a seed, with no pattern data to ship or maintain.
//
// ---------------------------------------------------------------------------
// THE PHASE CONTRACT
//
// **Step 0 is always an onset.** This is the load-bearing property of this
// file and it is asserted exhaustively in the tests, over every length to 64
// and every pulse count.
//
// It matters because "the Euclidean pattern for E(k,n)" does not name a single
// sequence. It names a NECKLACE -- a cyclic sequence -- and any rotation of it
// is equally entitled to the name. Three Chalkwalk projects independently grew
// Euclidean generators and two of them disagreed about the rotation, differing
// in 98 of the 120 patterns of length <= 16 while producing the identical
// rhythm in every one of them. A figure that landed on the downbeat in two
// projects landed off it in the third.
//
// Anchoring on step 0 settles it, for three reasons:
//
//   1. **A generator whose default misses the downbeat is surprising.** Users
//      reach for E(3,8) expecting the tresillo to start where the bar starts.
//   2. **It separates two controls that otherwise fight.** With a centred
//      phase -- what a textbook Bresenham line-drawing seed produces -- turning
//      a "pulses" knob ROTATES the figure as well as adding onsets, and does so
//      irregularly (for 32 steps as pulses go 1..8 the implied rotation is
//      16, 8, 26, 4, 28, 2, 6, 2, with no closed form). Anchored, pulses only
//      ever adds and redistributes onsets around a fixed downbeat, and `offset`
//      is the only thing that rotates. Two controls, two jobs.
//   3. **Nothing is lost.** Every rotation remains reachable through `offset`,
//      so a caller wanting a different phase asks for it explicitly instead of
//      inheriting one from an implementation detail.
//
// ---------------------------------------------------------------------------
// THE FORMULATION
//
//     onset at step i  iff  (i * pulses) % steps < pulses
//
// This is NOT Bjorklund's algorithm, whatever it may have been called
// elsewhere in this codebase's history. Bjorklund's is the recursive
// string-concatenation construction from the Euclidean-algorithm paper. This is
// the closed form, which produces the same necklaces, anchors on step 0, and
// -- the reason to prefer it -- yields an O(1) membership test as well as the
// vector, so audio-thread code needs no allocation.
//
// The names are worth being exact about, because one of them was wrong in two
// projects for years:
//
//     E(3,8) = x..x..x.   IS the tresillo.
//     E(5,8) = x.x.xx.x   is a ROTATION of the cinquillo (x.xx.xx.), not the
//                         cinquillo itself. Reachable with offset 6.
//
// ---------------------------------------------------------------------------
// JUCE-free, dependency-free, header-only. Do not add either.

namespace chalkwalk::music {

// The pattern as a vector, for callers that want to look at all of it.
// `offset` rotates: positive forward (right), negative backward.
[[nodiscard]] inline std::vector<bool> pattern(int steps, int pulses,
                                               int offset = 0) {
  if (steps <= 0)
    return {};
  if (pulses < 0)
    pulses = 0;
  if (pulses > steps)
    pulses = steps;

  std::vector<bool> result(static_cast<std::size_t>(steps), false);
  if (pulses == 0)
    return result;

  for (int i = 0; i < steps; ++i)
    result[static_cast<std::size_t>(i)] = ((i * pulses) % steps) < pulses;

  int rot = offset % steps;
  if (rot < 0)
    rot += steps;
  if (rot != 0) {
    // std::rotate shifts LEFT by k, so a right shift of `rot` is a left shift
    // of steps - rot.
    const int leftShift = steps - rot;
    std::rotate(result.begin(),
                result.begin() + static_cast<std::ptrdiff_t>(leftShift),
                result.end());
  }
  return result;
}

// Whether step `pos` is an onset, without building the pattern. Mirrors
// `pattern` exactly, including the rotation, and allocates nothing -- so it is
// safe on an audio thread. `pos` may be any integer; it wraps.
//
// The equivalence with `pattern` is asserted for every length, pulse count and
// rotation in the tests. A drift between the two would show as a sequencer
// whose display disagrees with what you hear, which is a miserable bug to find.
[[nodiscard]] inline bool hit(int pos, int steps, int pulses,
                              int offset = 0) noexcept {
  if (steps <= 0 || pulses <= 0)
    return false;
  if (pulses >= steps)
    return true;
  int rot = offset % steps;
  if (rot < 0)
    rot += steps;
  const int pmod = ((pos % steps) + steps) % steps;
  const int q = (pmod - rot + steps) % steps;
  return (q * pulses) % steps < pulses;
}

// How long the pattern takes to come round: steps / gcd(steps, pulses).
//
// This is the property that decides whether a figure moves or locks, and it is
// worth naming because it is easy to choose a pulse count by density alone and
// get a very different rhythm than intended. E(8,32) has period 4 -- eight
// repetitions of `x...` inside the bar, a metronome. E(9,32) has period 32 and
// takes the whole bar to return.
//
// NEITHER is better in general. A kick usually wants a short period: repetition
// is what makes it a pulse you can rely on. A bass line usually wants a long
// one, because a bass that repeats every four steps is not playing against the
// kick, it is doubling it. Choose deliberately.
[[nodiscard]] inline int patternPeriod(int steps, int pulses) noexcept {
  if (steps <= 0)
    return 0;
  if (pulses <= 0 || pulses >= steps)
    return 1;

  int a = steps, b = pulses;
  while (b != 0) {
    const int t = b;
    b = a % b;
    a = t;
  }
  return steps / a;
}

// The pulse count nearest `wanted` whose pattern spans all `steps` -- that is,
// coprime with them.
//
// For a caller that has decided it wants movement rather than lock. Ties go to
// the higher count when `preferAbove`, which is how a seed varies density
// without ever landing back on a repeating figure.
//
// Always terminates for steps > 1: `steps - 1` and 1 are both coprime with
// `steps`, so the outward search cannot run out of candidates.
[[nodiscard]] inline int nearestCoprimePulses(int steps, int wanted,
                                              bool preferAbove) noexcept {
  if (steps <= 1)
    return steps;
  if (wanted < 1)
    wanted = 1;
  if (wanted > steps - 1)
    wanted = steps - 1;

  for (int distance = 0; distance < steps; ++distance)
    for (int pass = 0; pass < 2; ++pass) {
      const bool high = preferAbove ? (pass == 0) : (pass == 1);
      const int candidate = high ? wanted + distance : wanted - distance;
      if (candidate < 1 || candidate > steps - 1)
        continue;
      if (patternPeriod(steps, candidate) == steps)
        return candidate;
      if (distance == 0)
        break; // both passes are the same candidate
    }
  return wanted;
}

inline constexpr int kOnsetVelocity = 64;
inline constexpr int kAccentedVelocity = 100;

// Velocities for each step: 0 for a rest, `kOnsetVelocity` or
// `kAccentedVelocity` for an onset. The accented onsets are themselves
// distributed Euclidean-wise OVER THE ONSETS -- not over the steps -- so
// accents fall in a pattern rather than on a fixed beat.
[[nodiscard]] inline std::vector<int> accents(int steps, int pulses, int offset,
                                              int numAccents) {
  const auto p = pattern(steps, pulses, offset);

  std::vector<int> onsetIdx;
  onsetIdx.reserve(p.size());
  for (int i = 0; i < static_cast<int>(p.size()); ++i)
    if (p[static_cast<std::size_t>(i)])
      onsetIdx.push_back(i);

  std::vector<int> result(p.size(), 0);
  if (onsetIdx.empty())
    return result;

  if (numAccents <= 0) {
    for (int idx : onsetIdx)
      result[static_cast<std::size_t>(idx)] = kOnsetVelocity;
    return result;
  }

  const int k = static_cast<int>(onsetIdx.size());
  if (numAccents > k)
    numAccents = k;
  const auto accentPat = pattern(k, numAccents, 0);

  for (int j = 0; j < k; ++j) {
    const int step = onsetIdx[static_cast<std::size_t>(j)];
    result[static_cast<std::size_t>(step)] =
        accentPat[static_cast<std::size_t>(j)] ? kAccentedVelocity
                                               : kOnsetVelocity;
  }
  return result;
}

} // namespace chalkwalk::music
