// SPDX-License-Identifier: MIT
#include "LegacyCheck.h"

#include <chalkwalk/music/Duration.h>

#include <string>

using namespace chalkwalk::music;

// ===========================================================================
// THE MIGRATION CONSTRAINT: the sequencer this ladder came from must not
// change. Its grid is sixteenths, so four units to the beat.
// ===========================================================================

TEST_CASE("the strength ladder reproduces the sequencer it came from") {
  const int expected[] = {1, 2, 3, 4, 4, 4};   // strengths 0..5, in sixteenths
  for (int strength = 0; strength <= 5; ++strength)
    CHECK_MSG(holdIn(holdForStrength(strength), 4) == expected[strength],
              "strength " + std::to_string(strength) + " gave " +
                  std::to_string(holdIn(holdForStrength(strength), 4)) +
                  " sixteenths, wanted " + std::to_string(expected[strength]));
}

TEST_CASE("stronger beats are never given less room") {
  for (int strength = 1; strength <= 6; ++strength)
    CHECK_MSG(holdForStrength(strength) >= holdForStrength(strength - 1),
              "the ladder went backwards at " + std::to_string(strength));
}

TEST_CASE("the ladder spans a real range") {
  // A model where every beat gets nearly the same length is not a model.
  CHECK_MSG(holdForStrength(3) >= 3 * holdForStrength(0),
            "a downbeat should be much longer than an off-beat");
}

// ===========================================================================
// The tier axis, which is the half the sequencer was missing.
// ===========================================================================

TEST_CASE("a destination may sit and a route may not") {
  for (Tier t : {Tier::ChordRoot, Tier::ChordTone, Tier::KeyRoot, Tier::ScaleTone})
    CHECK_MSG(holdForTier(t) == kHoldUncapped,
              "a welcome note should not be capped");
  for (Tier t : {Tier::ScaleClash, Tier::Chromatic, Tier::ChromaticClash})
    CHECK_MSG(holdForTier(t) < kTicksPerBeat,
              "a passing note should not be able to sit for a beat");
}

TEST_CASE("the tier cap can shorten a strong beat, which is the point") {
  // Holding a clash because it landed somewhere important is exactly the
  // failure this axis exists to prevent.
  CHECK_MSG(holdTicks(4, Tier::ScaleClash) < holdTicks(4, Tier::ChordTone),
            "a clash on a downbeat must not ring like a chord tone");
  CHECK_MSG(holdTicks(4, Tier::ScaleClash) == holdForTier(Tier::ScaleClash),
            "the tier should be the binding constraint there");
}

TEST_CASE("the beat cap can shorten a chord tone, which is also the point") {
  CHECK_MSG(holdTicks(0, Tier::ChordRoot) < holdTicks(3, Tier::ChordRoot),
            "an off-beat chord tone must not ring like a downbeat");
  CHECK_MSG(holdTicks(0, Tier::ChordRoot) == holdForStrength(0),
            "the beat should be the binding constraint there");
}

TEST_CASE("holdTicks is the smaller of the two, always") {
  for (int strength = 0; strength <= 5; ++strength)
    for (int t = 0; t <= 6; ++t) {
      const auto tier = static_cast<Tier>(t);
      const int got = holdTicks(strength, tier);
      CHECK_MSG(got <= holdForStrength(strength), "exceeded the beat's room");
      CHECK_MSG(got <= holdForTier(tier), "exceeded the note's tolerance");
      CHECK_MSG(got == holdForStrength(strength) || got == holdForTier(tier),
                "invented a value that is neither axis");
    }
}

// ===========================================================================
// Units. The reason this returns ticks at all.
// ===========================================================================

TEST_CASE("the ladder survives a coarse grid") {
  // The failure that forced ticks: rounded into eighths, a beat ladder of
  // 1 / 3-4 / 1-2 / 1-4 collapses to 2/1/1/0 and the weakest beat vanishes.
  // Every grid must keep the ordering and keep every note audible.
  for (int unitsPerBeat : {1, 2, 3, 4, 6, 8, 12, 16, 24, 48, 96, 480}) {
    int previous = 0;
    for (int strength = 0; strength <= 4; ++strength) {
      const int got = holdIn(holdForStrength(strength), unitsPerBeat);
      CHECK_MSG(got >= 1, "a note vanished at " + std::to_string(unitsPerBeat) +
                              " units per beat");
      CHECK_MSG(got >= previous, "the ladder inverted at " +
                                     std::to_string(unitsPerBeat) +
                                     " units per beat");
      previous = got;
    }
  }
}

TEST_CASE("a fine grid keeps the whole ladder distinct") {
  // At the resolution antiphon works in -- samples -- nothing is lost.
  const int perBeat = 48000 / 2;   // half a second a beat, at 48 kHz
  int seen[5];
  for (int strength = 0; strength <= 4; ++strength)
    seen[strength] = holdIn(holdForStrength(strength), perBeat);
  CHECK_MSG(seen[0] < seen[1] && seen[1] < seen[2] && seen[2] < seen[3],
            "the four rungs should be four different lengths");
  CHECK_MSG(seen[3] == seen[4], "and the top of the ladder is flat");
}

TEST_CASE("the tick base divides every subdivision either generator uses") {
  for (int d : {2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 96})
    CHECK_MSG(kTicksPerBeat % d == 0,
              "ticks per beat does not divide by " + std::to_string(d));
}

TEST_CASE("degenerate units are survivable") {
  CHECK_MSG(holdIn(holdForStrength(3), 0) == 0, "zero units per beat");
  CHECK_MSG(holdIn(holdForStrength(3), -4) == 0, "negative units per beat");
}
