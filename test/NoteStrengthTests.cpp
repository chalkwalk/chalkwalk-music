// SPDX-License-Identifier: MIT
#include "LegacyCheck.h"

#include <chalkwalk/music/NoteStrength.h>

#include <algorithm>
#include <string>
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

// With no chord the ORDER over scale tones must be what the scale-only model
// gave, because Lockstep's melody pool is scale-only and its output must not
// move. The absolute numbers change (they are now tier-offset); the ordering
// does not.
TEST_CASE("with no chord, the order over scale tones is unchanged",
          "[strength][contract]") {
  for (int root = 0; root < 12; ++root)
    for (int brightness = kLocrian; brightness <= kLydian; ++brightness) {
      const KeySig k{static_cast<uint8_t>(root), static_cast<int8_t>(brightness), {},
                     ScaleType::Diatonic};
      const uint16_t mask = pcMask(k);
      for (int a = 0; a < 12; ++a)
        for (int b = 0; b < 12; ++b) {
          if (!maskHas(mask, a) || !maskHas(mask, b))
            continue;
          INFO("root " << root << " brightness " << brightness << " pcs " << a
                       << " vs " << b);
          const bool oldOrder = noteStrengthRank(k, a) < noteStrengthRank(k, b);
          const bool newOrder = noteStrength(k, a) < noteStrength(k, b);
          REQUIRE(oldOrder == newOrder);
        }
    }
}

TEST_CASE("with no chart the ceiling ladder is the scale-only one",
          "[strength][contract]") {
  // The same SET of notes as before, expressed on the tier scale: a caller
  // with no chart only ever sees tiers 2, 3 and 5.
  CHECK(rankCeiling(4, false) == 3 * kTierStride + 2);
  CHECK(rankCeiling(3, false) == 3 * kTierStride + 2);
  CHECK(rankCeiling(2, false) == 3 * kTierStride + 4);
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
  REQUIRE(tierOf(cIonian, 0, chord) == Tier::ChordRoot);

}

TEST_CASE("the chord moves the ranking with it", "[strength][chord]") {
  // F is the avoid note over C major and a chord tone over D minor.
  const int fOverC = noteStrength(cIonian, 5, cMajor());
  const int fOverDm = noteStrength(cIonian, 5, dMinor());
  INFO("F over C " << fOverC << ", F over Dm " << fOverDm);
  REQUIRE(fOverDm < fOverC);
  REQUIRE(tierOf(cIonian, 5, dMinor()) == Tier::ChordTone);
}

// ===========================================================================
// The clash rule, derived rather than listed.
// ===========================================================================

TEST_CASE("Ionian's fourth clashes over the tonic chord", "[strength][clash]") {
  // F is a semitone above E, which C major is sounding.
  const int f = noteStrength(cIonian, 5, cMajor());
  INFO("F over C major ranks " << f);
  REQUIRE(tierOf(cIonian, 5, cMajor()) == Tier::ScaleClash);
}

// The reason to derive the rule rather than list avoid notes per mode: the
// same one fact gets Lydian right for free. The sharp-4 is a WHOLE tone above
// the third, so it does not clash -- and it is the characteristic note of the
// mode, so a per-mode list that demoted it would be actively wrong.
TEST_CASE("Lydian's sharp fourth is spared", "[strength][clash]") {
  const KeySig cLydian{0, kLydian, {}, ScaleType::Diatonic};
  const int fSharp = noteStrength(cLydian, 6, cMajor());
  INFO("F# over C major in Lydian ranks " << fSharp);
  REQUIRE(tierOf(cLydian, 6, cMajor()) != Tier::ScaleClash);
}

TEST_CASE("within the scale, a clash ranks worse than anything that does not",
          "[strength][clash]") {
  const auto chord = cMajor();
  const uint16_t mask = pcMask(cIonian);
  int worstNonClash = -1;
  int bestClash = 10000;

  for (int pc = 0; pc < 12; ++pc) {
    if (!maskHas(mask, pc))
      continue;
    const int r = noteStrength(cIonian, pc, chord);
    if (clashesWith(pc, chord))
      bestClash = std::min(bestClash, r);
    else
      worstNonClash = std::max(worstNonClash, r);
  }
  INFO("worst non-clash " << worstNonClash << ", best clash " << bestClash);
  REQUIRE(worstNonClash < bestClash);
}

// A DELIBERATE INVERSION, pinned so it cannot be "fixed" by accident.
//
// A scale tone that clashes outranks a chromatic that does not. Over C major
// that means F -- diatonic, and the textbook avoid note -- is preferred to Bb,
// which is out of key but a perfectly good blues colour. A case can be made
// either way; it is ordered like this because staying in key is the more
// reliable instinct for a generator that cannot hear itself, and because the
// gate keeps both of them off the strong beats anyway.
TEST_CASE("a diatonic clash still outranks a chromatic that does not clash",
          "[strength][clash]") {
  const auto chord = cMajor();
  const int f = noteStrength(cIonian, 5, chord);    // in key, clashes with E
  const int bFlat = noteStrength(cIonian, 10, chord);  // out of key, no clash

  REQUIRE(tierOf(cIonian, 5, chord) == Tier::ScaleClash);
  REQUIRE(tierOf(cIonian, 10, chord) == Tier::Chromatic);
  INFO("F " << f << ", Bb " << bFlat);
  REQUIRE(f < bFlat);

  // Neither is admissible on an ordinary beat, which is what limits the cost
  // of getting this call wrong.
  const int ceiling = rankCeiling(1, true);
  CHECK(f > ceiling);
  CHECK(bFlat > ceiling);
}

TEST_CASE("chromatics that clash are the last resort", "[strength][clash]") {
  const auto chord = cMajor();
  // C# is out of key and a semitone above C.
  REQUIRE(tierOf(cIonian, 1, chord) == Tier::ChromaticClash);
  for (int pc = 0; pc < 12; ++pc)
    if (pc != 1)
      CHECK(noteStrength(cIonian, pc, chord) < noteStrength(cIonian, 1, chord));
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
    const bool inKey = maskHas(pcMask(cIonian), pc);
    const bool clashes = clashesWith(pc, chord);
    const bool admitted = noteStrength(cIonian, pc, chord) <= ceiling;
    INFO("pc " << pc << " inKey " << inKey << " clashes " << clashes
               << " admitted " << admitted);
    CHECK(admitted == (inKey && !clashes));
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
    CHECK(noteStrength(cIonian, pc, none) == noteStrength(cIonian, pc));
}

TEST_CASE("strength stays finite and ordered for every input",
          "[strength][robustness]") {
  const auto chord = cMaj7();
  for (int root = 0; root < 12; ++root)
    for (int pc = -24; pc <= 36; ++pc) {
      const KeySig k{static_cast<uint8_t>(root), kDorian, {}, ScaleType::Diatonic};
      const int r = noteStrength(k, pc, chord);
      INFO("root " << root << " pc " << pc << " rank " << r);
      REQUIRE(r >= 0);
      REQUIRE(r <= 7 * kTierStride);
      // Wrapping an octave must not change the answer.
      REQUIRE(r == noteStrength(k, pc + 12, chord));
    }
}

// ===========================================================================
// The order, stated as a table over real changes. This is the specification:
// if the intent changes, this is the test that should be edited first.
// ===========================================================================

namespace {

std::string orderOver(const KeySig &key, const SoundingChord &chord, int howMany) {
  static const char *kNames[12] = {"C", "C#", "D",  "D#", "E",  "F",
                                   "F#", "G",  "G#", "A",  "A#", "B"};
  std::vector<int> pcs;
  for (int pc = 0; pc < 12; ++pc)
    pcs.push_back(pc);
  std::stable_sort(pcs.begin(), pcs.end(), [&](int a, int b) {
    return noteStrength(key, a, chord) < noteStrength(key, b, chord);
  });
  std::string out;
  for (int i = 0; i < howMany; ++i) {
    if (i)
      out += " ";
    out += kNames[pcs[static_cast<std::size_t>(i)]];
  }
  return out;
}

}  // namespace

TEST_CASE("the preference order over real changes", "[strength][order]") {
  // The chord root, then the chord tones by fifths, then the key root, then
  // the rest of the scale, then the avoid notes, then out of key.
  CHECK(orderOver(cIonian, cMajor(), 7) == "C G E D A B F");
  CHECK(orderOver(cIonian, cMaj7(), 7) == "C G E B D A F");
  CHECK(orderOver(cIonian, dMinor(), 7) == "D A F C G E B");

  // G7: the guide tones F and B are chord tones and rank above every scale
  // tone, and C -- the key root -- is correctly demoted, because over a
  // dominant it is the note to avoid.
  const auto g7 = chordOf(7, {0, 4, 7, 10});
  CHECK(orderOver(cIonian, g7, 7) == "G D F B A E C");
}

TEST_CASE("with no chord the order is the plain scale order",
          "[strength][order]") {
  CHECK(orderOver(cIonian, SoundingChord{}, 7) == "C G F D A E B");
}

// The key-root tier is currently order-redundant: fifths distance already
// gives the key root a within-tier rank of 0, which nothing else can reach.
// Asserted so that the redundancy is a known fact, and so that a change to the
// within-tier metric that breaks it shows up here rather than in a listening
// test six months later.
TEST_CASE("the key root sorts first among key-centred notes anyway",
          "[strength][order]") {
  for (int root = 0; root < 12; ++root)
    for (int brightness = kLocrian; brightness <= kLydian; ++brightness) {
      const KeySig k{static_cast<uint8_t>(root), static_cast<int8_t>(brightness), {},
                     ScaleType::Diatonic};
      const int rootRank = noteStrengthRank(k, root);
      REQUIRE(rootRank == 0);
      for (int pc = 0; pc < 12; ++pc) {
        if (pc == root)
          continue;
        INFO("root " << root << " brightness " << brightness << " pc " << pc);
        REQUIRE(noteStrengthRank(k, pc) > rootRank);
      }
    }
}
