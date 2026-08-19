// SPDX-License-Identifier: MIT
// Part of chalkwalk-music. See LICENSE.
#pragma once

// How long should this note ring?
//
// Two generators answered that with one axis each, and neither had the other's.
// A sequencer scaled sustain by BEAT STRENGTH -- a downbeat rings for a beat, an
// off-beat is short, and what is left over becomes the rest that bridges into
// the next onset. A bot following a chart capped by NOTE TIER -- a colour note
// passes rather than sits, because holding a semitone above a sounding chord
// tone is the difference between leaning into the clash and tripping over it.
//
// Both rules are right and they are about different things. Beat strength says
// how much room this MOMENT deserves; tier says how long this NOTE can bear to
// be heard for. So the answer is the smaller of the two, and each project gains
// the axis it was missing.
//
// ---------------------------------------------------------------------------
// WHY TICKS AND NOT STEPS
//
// The obvious signature returns a number of grid steps, and it is wrong,
// because a "step" is not a musical quantity. One generator's step is a
// sixteenth and the other's is an eighth, so the same 4 would mean a quarter
// note in one and a half note in the other -- and rounding a beat ladder into a
// coarse grid flattens it: on an eighth-note grid, "one beat / three quarters /
// half / quarter" collapses to 2/1/1/0.
//
// So this returns ticks at `kTicksPerBeat`, which divides by 2, 3, 4, 6, 8 and
// 12 exactly. The caller converts to whatever it counts in -- steps, samples,
// PPQ -- and clamps to its own gap, in its own units, because the gap to the
// next onset is the caller's fact and converting it here would only round it.

#include <chalkwalk/music/NoteStrength.h>

namespace chalkwalk::music {

// 96 per quarter note: the standard divisor, and exact for every subdivision
// either generator uses.
inline constexpr int kTicksPerBeat = 96;

// Effectively no limit, but a real number rather than a sentinel so callers can
// take a min without special-casing. Sixteen beats is longer than any phrase
// either generator produces.
inline constexpr int kHoldUncapped = 16 * kTicksPerBeat;

// How much room the BEAT deserves.
//
// The ladder a sequencer arrived at by ear: a strong beat rings for a whole
// one, and each step down halves the distance to a sixteenth. Kept exactly, so
// adopting this changes nothing for the generator it came from.
[[nodiscard]] inline constexpr int holdForStrength(int strength) noexcept {
  if (strength >= 3)
    return kTicksPerBeat;          // a full beat
  if (strength == 2)
    return kTicksPerBeat * 3 / 4;
  if (strength == 1)
    return kTicksPerBeat / 2;
  return kTicksPerBeat / 4;        // weakest: short, so a rest can bridge
}

// How long the NOTE can bear to be heard.
//
// Everything the harmony welcomes may sit: chord tones, the key's own root, any
// scale tone. Everything above that PASSES -- a clash or a chromatic is a way
// of getting somewhere, and holding it turns a passing note into a wrong one.
//
// Deliberately two-valued. A finer ladder here would be inventing distinctions
// the ear does not make: the question is not how dissonant the note is, it is
// whether it is a destination or a route.
[[nodiscard]] inline constexpr int holdForTier(Tier tier) noexcept {
  return tier <= Tier::ScaleTone ? kHoldUncapped : kTicksPerBeat / 2;
}

// The rule: the smaller of the two.
//
// The gap to the next onset is NOT applied here. It is the caller's fact, in
// the caller's units, and folding it in would mean converting it twice.
[[nodiscard]] inline constexpr int holdTicks(int strength, Tier tier) noexcept {
  const int byBeat = holdForStrength(strength);
  const int byNote = holdForTier(tier);
  return byBeat < byNote ? byBeat : byNote;
}

// Ticks in whatever the caller counts, given how many of those make a beat.
//
// Rounds down and never to zero: a note that was worth playing is worth being
// audible, and a caller on a coarse grid would otherwise get silence for its
// weakest beats.
[[nodiscard]] inline constexpr int holdIn(int ticks, int unitsPerBeat) noexcept {
  if (unitsPerBeat <= 0)
    return 0;
  const long long scaled =
      static_cast<long long>(ticks) * unitsPerBeat / kTicksPerBeat;
  return scaled < 1 ? 1 : static_cast<int>(scaled);
}

}  // namespace chalkwalk::music
