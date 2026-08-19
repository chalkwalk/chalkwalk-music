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
#include <cstdlib>
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

// Build a chord from a root and the intervals above it.
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

// ---------------------------------------------------------------------------
// THE ORDER
//
// Seven tiers. The tier decides everything; fifths distance only ever breaks
// ties WITHIN a tier. That is deliberate rather than a simplification: the
// point of favouring chord tones is to push a line into playing the changes
// instead of noodling an in-key pentatonic over the whole form, and a weighted
// sum lets a conveniently-placed scale tone outbid a chord tone, which is
// exactly the behaviour being designed out.
//
//   0  the chord root
//   1  the other chord tones          -- fifths from the CHORD root
//   2  the key root                   -- unless it clashes, see below
//   3  scale tones that do not clash  -- fifths from the KEY root
//   4  scale tones that clash
//   5  chromatics that do not clash
//   6  chromatics that clash
//
// A scale tone that clashes still outranks a chromatic that does not. That is
// a deliberate choice and a debatable one -- over C major, F is diatonic and
// the textbook avoid note, while Bb is out of key but a perfectly good blues
// colour. It is ordered this way because staying in key is the more reliable
// instinct for a generator that cannot hear itself.
//
// CLASH MEANS A SEMITONE ABOVE, not either side. The asymmetry is real: a note
// a semitone above a chord tone hangs as an unresolved suspension of it, while
// a semitone below reads as a leading tone into it. Symmetric adjacency would
// demote the maj7 over a major triad, the 9th over a minor chord and the 13th
// over a dominant -- three sounds that are entirely standard. Roughness IS
// symmetric, but roughness is a voicing question and depends on register,
// which this pitch-class model cannot see at all. See ROADMAP.md.
//
// THE KEY-ROOT TIER IS CURRENTLY ORDER-REDUNDANT, and that is worth knowing
// before someone deletes it as dead weight. Fifths distance from the key root
// gives the key root itself a within-tier rank of 0, which no other note can
// reach -- the brightness lean subtracts at most 1 from a distance of at least
// 2 -- so the key root already sorts first among the key-centred tiers without
// a tier of its own. It is kept because it states the intent, and because a
// future within-tier metric that is not fifths-based would need it. There is a
// test asserting the equivalence, so the redundancy is a known fact rather
// than an accident.
//
// The key root loses its tier when it clashes: over G7 in C major the note C
// is both the key root and a semitone above the B, and it is the classic note
// to avoid there. Tier beats provenance.
//
// WITHIN CHORD TONES the order is fifths from the chord root, so the root and
// fifth come before the third and seventh. Those last two are the guide tones
// that actually define a change, so an ordering that prefers roots would sound
// like outlining rather than playing changes -- but the gate admits ALL chord
// tones on a strong beat, so this ordering only decides ties, never what is
// reachable. If a ceiling is ever tightened to cut inside the chord, revisit.

// One tier is worth more than any distance within a tier.
inline constexpr int kTierStride = 16;   // > the widest possible fifths rank (12)

enum class Tier : int {
  ChordRoot      = 0,
  ChordTone      = 1,
  KeyRoot        = 2,
  ScaleTone      = 3,
  ScaleClash     = 4,
  Chromatic      = 5,
  ChromaticClash = 6,
};

// Is `pc` a semitone ABOVE something the chord is sounding? A chord tone never
// is: in a major seventh the root sits a semitone above the seventh, and
// testing this before membership would demote the root of every maj7.
[[nodiscard]] inline bool clashesWith(int pitchClass,
                                      const SoundingChord &chord) noexcept {
  const int pc = ((pitchClass % 12) + 12) % 12;
  if (!chord.present() || maskHas(chord.tones, pc))
    return false;
  return maskHas(chord.tones, ((pc - 1) % 12 + 12) % 12);
}

[[nodiscard]] inline Tier tierOf(const KeySig &key, int pitchClass,
                                 const SoundingChord &chord) noexcept {
  const int pc = ((pitchClass % 12) + 12) % 12;

  if (chord.present()) {
    if (pc == chord.root)
      return Tier::ChordRoot;
    if (maskHas(chord.tones, pc))
      return Tier::ChordTone;
  }

  const bool inScale = maskHas(pcMask(key), pc);
  const bool clash = clashesWith(pc, chord);

  if (clash)
    return inScale ? Tier::ScaleClash : Tier::ChromaticClash;
  if (pc == (((key.root % 12) + 12) % 12))
    return Tier::KeyRoot;
  return inScale ? Tier::ScaleTone : Tier::Chromatic;
}

// Lower is stronger. Zero is the root of the sounding chord, or the tonic when
// nothing is sounding.
[[nodiscard]] inline int noteStrength(const KeySig &key, int pitchClass,
                                      const SoundingChord &chord = {}) noexcept {
  const int pc = ((pitchClass % 12) + 12) % 12;
  const Tier tier = tierOf(key, pc, chord);

  // Chord tones are organised around the chord; everything else around the key.
  // The brightness lean is a property of the SCALE, so it applies only to the
  // key-centred distances -- leaning a chord-relative distance by the scale's
  // brightness would be mixing two unrelated facts.
  const int within = (tier == Tier::ChordRoot || tier == Tier::ChordTone)
                         ? 2 * std::abs(fifthsOffsetOf(chord.root, pc))
                         : noteStrengthRank(key, pc);

  return static_cast<int>(tier) * kTierStride + within;
}

// The worst rank a note may have at this metric strength.
//
// Two ladders, because the two contexts ask different questions, and each is
// preserved exactly as the project it came from had it.
//
// WITH a chart, strength buys chord agreement: a strong beat takes a chord
// tone, an ordinary beat anything in key that does not clash, and only an
// off-beat may touch a clash or a chromatic -- and then in passing, which is
// the caller's business (cap the duration).
//
// WITHOUT one, strength buys tonal closeness: a downbeat takes the tonic or
// its nearest fifths, a quarter reaches out to the pentatonic arc, anything
// weaker takes any colour note. Expressed against the tier scale, this is the
// identical set of notes the scale-only model admitted, because a caller with
// no chart only ever sees tiers 2, 3 and 5.
[[nodiscard]] inline int rankCeiling(int strength, bool hasChart) noexcept {
  if (hasChart) {
    if (strength >= 3)
      return 2 * kTierStride - 1;   // chord tones only
    if (strength >= 1)
      return 4 * kTierStride - 1;   // in key, and not clashing
    return 1000;                    // off-beat: colour is allowed
  }
  if (strength >= 3)
    return 3 * kTierStride + 2;     // tonic and its nearest fifths
  if (strength == 2)
    return 3 * kTierStride + 4;     // out to the pentatonic arc
  return 1000;
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
