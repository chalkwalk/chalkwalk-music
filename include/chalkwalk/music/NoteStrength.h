// SPDX-License-Identifier: MIT
// Part of chalkwalk-music. See LICENSE.
#pragma once

// How good is this note, here, right now?
//
// Every generative melody line answers that question, and two projects
// answered it differently because they had different information. A sequencer
// track has a key and no chord chart, so it can only ask "how consonant is
// this note in this scale". A bot following a progression knows what is
// sounding underneath, so it can ask a sharper question. Neither model can do
// the other's job, and the merge is not a compromise between them -- it is one
// function with an optional second input.
//
// ---------------------------------------------------------------------------
// THE MODEL: FOUR AXES, AND WHY EACH IS SEPARATE
//
// 1. MEMBERSHIP. Out of the scale entirely is weakest, regardless of anything
//    below. Handled by the caller, which builds its candidate pool from the
//    scale mask.
//
// 2. CHORD MEMBERSHIP -- SET MEMBERSHIP, NOT DISTANCE. If a chord is sounding,
//    its tones are the strong notes. This is the axis it is easiest to get
//    wrong: ranking by fifths distance from the CHORD ROOT looks equivalent
//    and is not. In Cmaj7 the E and B are chord tones and completely
//    consonant, but they sit four and five fifths from C -- a distance metric
//    would rank the major third and seventh of the sounding chord as weak
//    notes, which is backwards.
//
// 3. TONAL DISTANCE from the SCALE root, in fifths, with a brightness lean.
//    This orders everything the chord does not settle, and it is the entire
//    ranking when there is no chart. `((pc - root) * 7) mod 12` inverts the
//    "a fifth is seven semitones" map, so a pitch class becomes its position
//    on the circle of fifths in one line, and "further in fifths is weaker"
//    falls out of the geometry. The lean is the part with no equivalent
//    anywhere else: at equal distance a bright scale favours the sharp-side
//    note and a dark scale the flat-side one, because the sharp-4 belongs to
//    Lydian and the flat-2 to Phrygian.
//
// 4. CLASH. A semitone above a note the chord is actually SOUNDING. This is
//    orthogonal to the other three -- it is about simultaneity, not tonality --
//    which is why it correctly spares Lydian's sharp-4 (a whole tone above the
//    third) while condemning Ionian's fourth. Deriving it beats listing avoid
//    notes per mode: the list is long, and it is the same fact each time.
//
//    **A CHORD TONE IS NEVER A CLASH.** In a major seventh the root sits a
//    semitone above the seventh; in a dominant seventh so does nothing, but
//    add a ninth and the pattern repeats. Testing membership before clash is
//    what stops the model demoting the root of every maj7 chord, and it is
//    asserted in the tests because a plausible reordering breaks it silently.
//
// ---------------------------------------------------------------------------
// DEGRADING TO THE SCALE-ONLY MODEL EXACTLY
//
// With no chord, axes 2 and 4 contribute nothing and `noteStrength` returns
// precisely what a fifths-distance-plus-lean model returns. That is not a
// happy accident, it is the design constraint: adopting this must not change
// what a sequencer with no chart already plays. There is a test that pins it.
//
// ---------------------------------------------------------------------------
// THE GATE IS THE OTHER HALF
//
// Both generators this came from compute a rank and then GATE it by metric
// strength: a strong beat may only take a strong note, a weak one may take
// anything. That coupling is the part that matters most musically. Porting a
// beat-strength axis WITHOUT it is what made one of these projects' minor keys
// sound wrong -- the flat sixth was as welcome on beat three as the fifth was.
//
// The ceiling depends on whether a chart exists, because the question does. See
// `rankCeiling`.

#include <chalkwalk/music/Scale.h>

#include <cstdint>
#include <vector>

namespace chalkwalk::music {

// What the harmony is doing right now, reduced to the two facts ranking needs.
//
// Deliberately minimal. Chord naming, quality, roman numerals and voice leading
// are real problems, but they are entangled with diatonic spelling and belong
// with whoever owns the chart. This carries what a note-ranker needs and
// nothing else, so a caller with no chord model at all can pass `{}`.
struct SoundingChord {
  int root = -1;       // pitch class of the chord root; negative means no chart
  uint16_t tones = 0;  // pitch classes actually sounding, as a 12-bit mask

  [[nodiscard]] bool present() const noexcept { return root >= 0 && tones != 0; }
};

// Rank bands. A chord tone always outranks a non-chord tone, and a non-chord
// tone always outranks a clash, whatever the fifths distance -- so the bands
// are spaced wider than the widest possible fifths rank (which is 12).
inline constexpr int kMaxFifthsRank = 12;
inline constexpr int kNonChordTonePenalty = 16;
inline constexpr int kClashPenalty = 32;

// Build a chord from a root and its intervals above that root.
[[nodiscard]] inline SoundingChord chordOf(int root,
                                           const std::vector<int> &intervals) {
  SoundingChord c;
  if (root < 0)
    return c;
  c.root = ((root % 12) + 12) % 12;
  for (int i : intervals)
    c.tones = setPc(c.tones, c.root + i);
  return c;
}

// Lower is stronger. Zero is the tonic with nothing sounding against it.
[[nodiscard]] inline int noteStrength(const KeySig &key, int pitchClass,
                                      const SoundingChord &chord = {}) noexcept {
  const int pc = ((pitchClass % 12) + 12) % 12;

  // Axis 3: the scale-relative ranking. Always present, and on its own when
  // there is no chart.
  int rank = noteStrengthRank(key, pc);

  if (!chord.present())
    return rank;

  // Axis 2: membership, tested FIRST so that a chord tone can never be
  // demoted as a clash.
  if (maskHas(chord.tones, pc))
    return rank;

  rank += kNonChordTonePenalty;

  // Axis 4: a semitone above something the chord is sounding.
  const int below = ((pc - 1) % 12 + 12) % 12;
  if (maskHas(chord.tones, below))
    rank += kClashPenalty;

  return rank;
}

// The worst rank a note may have at this metric strength.
//
// Two ladders, because the two contexts ask different questions and each is
// preserved exactly as its project had it.
//
// WITHOUT a chart, strength buys tonal closeness: a downbeat takes the root or
// its nearest fifths, a quarter reaches out to the pentatonic arc, anything
// weaker takes any colour note.
//
// WITH a chart, strength buys chord agreement: a strong beat takes a chord
// tone, an ordinary beat any non-clashing note, and only an off-beat may touch
// a semitone above a chord tone -- and then only in passing, which is the
// caller's business (cap the duration).
[[nodiscard]] inline int rankCeiling(int strength, bool hasChart) noexcept {
  if (hasChart) {
    if (strength >= 3)
      return kNonChordTonePenalty - 1;  // chord tones only
    if (strength >= 1)
      return kClashPenalty - 1;         // anything that does not clash
    return 1000;                        // off-beat: colour is allowed
  }
  if (strength >= 3)
    return 2;     // downbeat / half-bar -- root and nearest fifths
  if (strength == 2)
    return 4;     // quarter -- out to the pentatonic arc
  return 1000;    // eighth / off-beat -- any colour note
}

// Search outward from `idx0` for the nearest candidate whose rank the beat
// allows, ties resolving downward.
//
// Cleaner than building an allowed-set and scanning it, and the tie-break is
// defined rather than incidental -- which matters, because a generator whose
// ties fall differently on two runs is not reproducible from a seed.
[[nodiscard]] inline std::size_t snapToRank(const std::vector<int> &ranks,
                                            std::size_t idx0,
                                            int ceiling) noexcept {
  const auto n = static_cast<std::ptrdiff_t>(ranks.size());
  if (n == 0)
    return idx0;
  const auto start = static_cast<std::ptrdiff_t>(idx0);

  for (std::ptrdiff_t d = 0; d < n; ++d) {
    const std::ptrdiff_t lo = start - d;
    const std::ptrdiff_t hi = start + d;
    if (lo >= 0 && ranks[static_cast<std::size_t>(lo)] <= ceiling)
      return static_cast<std::size_t>(lo);
    if (hi < n && ranks[static_cast<std::size_t>(hi)] <= ceiling)
      return static_cast<std::size_t>(hi);
  }
  return idx0;
}

}  // namespace chalkwalk::music
