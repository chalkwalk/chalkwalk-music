// SPDX-License-Identifier: MIT
#include "LegacyCheck.h"

#include <chalkwalk/music/NoteStrength.h>

#include <vector>

using namespace chalkwalk::music;

namespace {

// C major / Ionian, the reference key for most of these.
const KeySig cIonian{0, kIonian, {}, ScaleType::Diatonic};

SoundingChord cMajor()  { return chordOf(0, {0, 4, 7}); }
SoundingChord cMaj7()   { return chordOf(0, {0, 4, 7, 11}); }
SoundingChord dMinor()  { return chordOf(2, {0, 3, 7}); }

}  // namespace

// ===========================================================================
// The constraint that made this mergeable: with no chart, nothing changes.
// ===========================================================================

TEST_CASE("with no chord, strength is exactly the scale-only ranking",
          "[strength][contract]") {
  for (int root = 0; root < 12; ++root)
    for (int brightness = kLocrian; brightness <= kLydian; ++brightness) {
      const KeySig k{root, static_cast<int8_t>(brightness), {},
                     ScaleType::Diatonic};
      for (int pc = 0; pc < 12; ++pc) {
        INFO("root " << root << " brightness " << brightness << " pc " << pc);
        REQUIRE(noteStrength(k, pc) == noteStrengthRank(k, pc));
        REQUIRE(noteStrength(k, pc, SoundingChord{}) == noteStrengthRank(k, pc));
      }
    }
}

TEST_CASE("with no chart the ceiling ladder is the scale-only one",
          "[strength][contract]") {
  CHECK(rankCeiling(4, false) == 2);
  CHECK(rankCeiling(3, false) == 2);
  CHECK(rankCeiling(2, false) == 4);
  CHECK(rankCeiling(1, false) == 1000);
  CHECK(rankCeiling(0, false) == 1000);
}

// ===========================================================================
// Chord membership -- the axis it is easiest to get wrong.
// ===========================================================================

TEST_CASE("every chord tone outranks every non-chord tone",
          "[strength][chord]") {
  const auto chord = cMajor();
  int worstChordTone = -1;
  int bestNonChordTone = 10000;

  for (int pc = 0; pc < 12; ++pc) {
    const int r = noteStrength(cIonian, pc, chord);
    if (maskHas(chord.tones, pc))
      worstChordTone = std::max(worstChordTone, r);
    else
      bestNonChordTone = std::min(bestNonChordTone, r);
  }
  INFO("worst chord tone " << worstChordTone << ", best other "
                           << bestNonChordTone);
  REQUIRE(worstChordTone < bestNonChordTone);
}

// The whole reason membership beats distance. In Cmaj7 the E and B sit four
// and five fifths from C, so a fifths-from-the-chord-root model ranks the
// major third and seventh of the sounding chord as weak notes.
TEST_CASE("the third and seventh of a maj7 are strong, not distant",
          "[strength][chord]") {
  const auto chord = cMaj7();
  const int e = noteStrength(cIonian, 4, chord);   // major third
  const int b = noteStrength(cIonian, 11, chord);  // major seventh
  const int d = noteStrength(cIonian, 2, chord);   // a plain scale tone
  const int f = noteStrength(cIonian, 5, chord);   // the avoid note in Ionian

  INFO("E " << e << " B " << b << " D " << d << " F " << f);
  REQUIRE(e < d);
  REQUIRE(b < d);
  REQUIRE(d < f);
}

// THE maj7 TRAP. C is a semitone above the sounding B, and C is the root. A
// clash test applied before membership demotes the root of every major
// seventh chord -- a plausible reordering that breaks nothing visibly.
TEST_CASE("a chord tone is never treated as a clash", "[strength][chord]") {
  const auto chord = cMaj7();
  REQUIRE(maskHas(chord.tones, 0));   // C is in the chord
  REQUIRE(maskHas(chord.tones, 11));  // ...and so is the B below it

  const int c = noteStrength(cIonian, 0, chord);
  INFO("C in Cmaj7 ranks " << c);
  REQUIRE(c < kNonChordTonePenalty);
  REQUIRE(c == noteStrengthRank(cIonian, 0));  // undemoted
}

TEST_CASE("the chord moves the ranking with it", "[strength][chord]") {
  // F is the avoid note over C major and a chord tone over D minor.
  const int fOverC = noteStrength(cIonian, 5, cMajor());
  const int fOverDm = noteStrength(cIonian, 5, dMinor());
  INFO("F over C " << fOverC << ", F over Dm " << fOverDm);
  REQUIRE(fOverDm < fOverC);
  REQUIRE(fOverDm < kNonChordTonePenalty);
}

// ===========================================================================
// The clash rule, derived rather than listed.
// ===========================================================================

TEST_CASE("Ionian's fourth clashes over the tonic chord", "[strength][clash]") {
  // F is a semitone above E, which C major is sounding.
  const int f = noteStrength(cIonian, 5, cMajor());
  INFO("F over C major ranks " << f);
  REQUIRE(f >= kNonChordTonePenalty + kClashPenalty);
}

// The reason to derive the rule rather than list avoid notes per mode: the
// same one fact gets Lydian right for free. The sharp-4 is a WHOLE tone above
// the third, so it does not clash -- and it is the characteristic note of the
// mode, so a per-mode list that demoted it would be actively wrong.
TEST_CASE("Lydian's sharp fourth is spared", "[strength][clash]") {
  const KeySig cLydian{0, kLydian, {}, ScaleType::Diatonic};
  const int fSharp = noteStrength(cLydian, 6, cMajor());
  INFO("F# over C major in Lydian ranks " << fSharp);
  REQUIRE(fSharp < kNonChordTonePenalty + kClashPenalty);
}

TEST_CASE("a clash always ranks worse than any non-clashing note",
          "[strength][clash]") {
  const auto chord = cMajor();
  int worstNonClash = -1;
  int bestClash = 10000;

  for (int pc = 0; pc < 12; ++pc) {
    const int r = noteStrength(cIonian, pc, chord);
    const int below = ((pc - 1) % 12 + 12) % 12;
    const bool clashes = !maskHas(chord.tones, pc) && maskHas(chord.tones, below);
    if (clashes)
      bestClash = std::min(bestClash, r);
    else
      worstNonClash = std::max(worstNonClash, r);
  }
  INFO("worst non-clash " << worstNonClash << ", best clash " << bestClash);
  REQUIRE(worstNonClash < bestClash);
}

// ===========================================================================
// The gate.
// ===========================================================================

TEST_CASE("a strong beat with a chart admits chord tones and nothing else",
          "[strength][gate]") {
  const auto chord = cMajor();
  const int ceiling = rankCeiling(4, true);
  for (int pc = 0; pc < 12; ++pc) {
    const bool admitted = noteStrength(cIonian, pc, chord) <= ceiling;
    INFO("pc " << pc << " admitted " << admitted);
    CHECK(admitted == maskHas(chord.tones, pc));
  }
}

TEST_CASE("an ordinary beat admits anything that does not clash",
          "[strength][gate]") {
  const auto chord = cMajor();
  const int ceiling = rankCeiling(1, true);
  for (int pc = 0; pc < 12; ++pc) {
    const int below = ((pc - 1) % 12 + 12) % 12;
    const bool clashes = !maskHas(chord.tones, pc) && maskHas(chord.tones, below);
    const bool admitted = noteStrength(cIonian, pc, chord) <= ceiling;
    INFO("pc " << pc << " clashes " << clashes << " admitted " << admitted);
    CHECK(admitted == !clashes);
  }
}

TEST_CASE("an off-beat admits everything", "[strength][gate]") {
  const auto chord = cMajor();
  const int ceiling = rankCeiling(0, true);
  for (int pc = 0; pc < 12; ++pc)
    CHECK(noteStrength(cIonian, pc, chord) <= ceiling);
}

// ===========================================================================
// snapToRank
// ===========================================================================

TEST_CASE("snapToRank finds the nearest admissible candidate",
          "[strength][snap]") {
  //            0  1  2  3  4  5
  std::vector<int> ranks{9, 9, 9, 0, 9, 2};
  CHECK(snapToRank(ranks, 0, 2) == 3u);
  CHECK(snapToRank(ranks, 5, 2) == 5u);
  CHECK(snapToRank(ranks, 4, 2) == 3u);  // tie at distance 1: downward wins
}

TEST_CASE("snapToRank resolves ties downward, every time", "[strength][snap]") {
  std::vector<int> ranks{0, 9, 0};
  for (int i = 0; i < 20; ++i)
    CHECK(snapToRank(ranks, 1, 0) == 0u);
}

TEST_CASE("snapToRank returns the starting point when nothing qualifies",
          "[strength][snap]") {
  std::vector<int> ranks{9, 9, 9};
  CHECK(snapToRank(ranks, 1, 0) == 1u);
  CHECK(snapToRank({}, 0, 0) == 0u);
}

// ===========================================================================
// chordOf
// ===========================================================================

TEST_CASE("chordOf builds a pitch-class mask from intervals",
          "[strength][chord]") {
  const auto c = chordOf(0, {0, 4, 7});
  CHECK(c.present());
  CHECK(c.root == 0);
  CHECK(maskHas(c.tones, 0));
  CHECK(maskHas(c.tones, 4));
  CHECK(maskHas(c.tones, 7));
  CHECK_FALSE(maskHas(c.tones, 2));

  // Intervals wrap, and a root outside 0..11 is folded.
  const auto wrapped = chordOf(14, {0, 13});
  CHECK(wrapped.root == 2);
  CHECK(maskHas(wrapped.tones, 2));
  CHECK(maskHas(wrapped.tones, 3));
}

TEST_CASE("a chord with no root is absent, and ranks nothing",
          "[strength][chord]") {
  const SoundingChord none;
  CHECK_FALSE(none.present());
  CHECK_FALSE(chordOf(-1, {0, 4, 7}).present());
  for (int pc = 0; pc < 12; ++pc)
    CHECK(noteStrength(cIonian, pc, none) == noteStrengthRank(cIonian, pc));
}

TEST_CASE("strength stays finite and ordered for every input",
          "[strength][robustness]") {
  const auto chord = cMaj7();
  for (int root = 0; root < 12; ++root)
    for (int pc = -24; pc <= 36; ++pc) {
      const KeySig k{root, kDorian, {}, ScaleType::Diatonic};
      const int r = noteStrength(k, pc, chord);
      INFO("root " << root << " pc " << pc << " rank " << r);
      REQUIRE(r >= 0);
      REQUIRE(r <= kMaxFifthsRank + kNonChordTonePenalty + kClashPenalty);
      // Wrapping an octave must not change the answer.
      REQUIRE(r == noteStrength(k, pc + 12, chord));
    }
}
