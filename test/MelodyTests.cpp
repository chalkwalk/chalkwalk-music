// SPDX-License-Identifier: MIT
#include "LegacyCheck.h"

#include <chalkwalk/music/Melody.h>
#include <chalkwalk/music/NoteStrength.h>

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

using namespace chalkwalk::music;

namespace {

const KeySig cIonian{0, kIonian, {}, ScaleType::Diatonic};

// The pool antiphon's lead builds: the scale across three octaves.
std::vector<int> scalePool(const KeySig &key, int loOctave, int hiOctave) {
  std::vector<int> out;
  const auto mask = pcMask(key);
  for (int octave = loOctave; octave <= hiOctave; ++octave)
    for (int pc = 0; pc < 12; ++pc)
      if (maskHas(mask, pc))
        out.push_back(12 * octave + pc);
  std::sort(out.begin(), out.end());
  return out;
}

std::vector<int> ranksFor(const KeySig &key, const std::vector<int> &pool,
                          const SoundingChord &chord) {
  std::vector<int> r(pool.size());
  for (std::size_t i = 0; i < pool.size(); ++i)
    r[i] = noteStrength(key, pool[i] % 12, chord);
  return r;
}

}  // namespace

// ===========================================================================
// The cost table. Exhaustive, because it is small enough to be, and because
// the ORDER between bands is the whole design -- a table that is merely
// plausible per-entry can still have the fifth costing more than the tritone.
// ===========================================================================

TEST_CASE("interval cost is symmetric in direction") {
  for (int d = -36; d <= 36; ++d)
    CHECK_MSG(intervalCost(d) == intervalCost(-d),
              "cost of " + std::to_string(d) + " differs by sign");
}

TEST_CASE("stepwise motion is free") {
  for (int d : {-2, -1, 1, 2})
    CHECK_MSG(intervalCost(d) == 0, "step of " + std::to_string(d) + " is not free");
}

TEST_CASE("standing still is not the cheapest move") {
  // The unison is not motion. If it is free then every other term in the
  // objective becomes an argument for repeating the note, and the melody
  // stops being one -- measured at 54% repeated notes with a direction weight
  // of 2, against 20% once the unison is priced.
  CHECK_MSG(intervalCost(0) > intervalCost(1), "a repeat must cost more than a step");
  CHECK_MSG(intervalCost(0) > intervalCost(2), "and more than a whole tone");
  CHECK_MSG(intervalCost(0) <= intervalCost(4), "but no more than a third");
}

TEST_CASE("the cost is deliberately not monotone in size") {
  // This is the property that distinguishes the model from "bigger is worse",
  // and every one of these would hold with the dips removed except these.
  CHECK_MSG(intervalCost(7) < intervalCost(6), "the fifth must beat the tritone");
  CHECK_MSG(intervalCost(5) < intervalCost(6), "the fourth must beat the tritone");
  CHECK_MSG(intervalCost(12) < intervalCost(11), "the octave must beat the seventh");
  CHECK_MSG(intervalCost(12) < intervalCost(9), "the octave must beat the sixth");
}

TEST_CASE("the perfect intervals cost the same") {
  // The one place this table disagrees with the ear that specified it: the
  // fourth was grouped with the tritone, and a perfect fourth is as idiomatic
  // a leap as a fifth. Pinned so the disagreement is visible if it is revisited.
  CHECK_MSG(intervalCost(5) == intervalCost(7), "fourth and fifth must match");
}

TEST_CASE("beyond an octave the cost grows without bound") {
  for (int d = 13; d < 40; ++d)
    CHECK_MSG(intervalCost(d) > intervalCost(d - 1),
              "cost stopped growing at " + std::to_string(d));
  CHECK_MSG(intervalCost(13) > intervalCost(12), "a ninth must beat an octave");
}

TEST_CASE("no interval ever costs less than nothing") {
  for (int d = 0; d <= 36; ++d)
    CHECK_MSG(intervalCost(d) >= 0, "negative cost at " + std::to_string(d));
}

// ===========================================================================
// THE GATE IS A HARD CONSTRAINT. This is the regression that would actually
// hurt: the objective rewrite is exactly where an admissibility check can slip,
// and the result would still sound nearly right, which is worse.
// ===========================================================================

TEST_CASE("no note above the ceiling is ever chosen") {
  const auto pool = scalePool(cIonian, 4, 6);
  const auto chord = chordOf(0, {0, 4, 7});
  const auto ranks = ranksFor(cIonian, pool, chord);

  for (int strength = 0; strength <= 4; ++strength) {
    const int ceiling = rankCeiling(strength, /*hasChart=*/true);
    // Some candidate must actually satisfy the gate, or this proves nothing.
    const bool anyAdmissible =
        std::any_of(ranks.begin(), ranks.end(),
                    [&](int r) { return r <= ceiling; });
    CHECK_MSG(anyAdmissible,
              "no admissible candidate at strength " + std::to_string(strength));

    for (int aim = 48; aim <= 96; ++aim)
      for (int last = -1; last <= 96; last += 7) {
        const auto idx = chooseNote(pool, ranks, ceiling, aim, MelodyState{last, 0});
        CHECK_MSG(ranks[idx] <= ceiling,
                  "strength " + std::to_string(strength) + " aim " +
                      std::to_string(aim) + " last " + std::to_string(last) +
                      " chose rank " + std::to_string(ranks[idx]));
      }
  }
}

TEST_CASE("with no admissible candidate the objective still governs") {
  const std::vector<int> pool{60, 62, 64};
  const std::vector<int> ranks{900, 900, 900};
  // Nothing passes; the fallback must be the best note by the objective, not
  // an arbitrary index.
  CHECK_MSG(chooseNote(pool, ranks, 10, 64, MelodyState{-1, 0}) == 2, "fallback ignored the aim");
}

// ===========================================================================
// The objective itself.
// ===========================================================================

TEST_CASE("with no previous note this is pure contour following") {
  const auto pool = scalePool(cIonian, 4, 6);
  const std::vector<int> open(pool.size(), 0);
  for (int aim = 50; aim <= 94; ++aim) {
    const auto idx = chooseNote(pool, open, 1000, aim, MelodyState{});
    for (std::size_t i = 0; i < pool.size(); ++i)
      CHECK_MSG(std::abs(pool[idx] - aim) <= std::abs(pool[i] - aim),
                "not nearest to aim " + std::to_string(aim));
  }
}

TEST_CASE("a weight of zero degrades to the old nearest-to-contour behaviour") {
  const auto pool = scalePool(cIonian, 4, 6);
  const std::vector<int> open(pool.size(), 0);
  const MelodyWeights contourOnly{1, 0, 0};
  for (int aim = 50; aim <= 94; ++aim)
    for (int last = 48; last <= 96; last += 5) {
      const auto idx = chooseNote(pool, open, 1000, aim, MelodyState{last, 0}, contourOnly);
      const auto none = chooseNote(pool, open, 1000, aim, MelodyState{-1, 0});
      CHECK_MSG(idx == none, "interval weight 0 changed the choice at aim " +
                                 std::to_string(aim));
    }
}

TEST_CASE("the interval term pulls the line toward the previous note") {
  const auto pool = scalePool(cIonian, 4, 6);
  const std::vector<int> open(pool.size(), 0);

  // Aim exactly between two admissible notes an octave apart in usefulness:
  // the contour is indifferent, so the previous note decides.
  const int aim = 72;
  const auto fromBelow = chooseNote(pool, open, 1000, aim, MelodyState{62, 0});
  const auto fromAbove = chooseNote(pool, open, 1000, aim, MelodyState{84, 0});
  CHECK_MSG(pool[fromBelow] <= pool[fromAbove],
            "approaching from below did not land lower");
}

TEST_CASE("leaps shrink across a whole line, which is the point") {
  // The statistical claim, made the only way it can be: generate a line under
  // both objectives from the same contour and compare total motion.
  const auto pool = scalePool(cIonian, 4, 6);
  const auto chord = chordOf(0, {0, 4, 7});
  const auto ranks = ranksFor(cIonian, pool, chord);
  const int ceiling = rankCeiling(0, /*hasChart=*/true);

  // A deliberately jagged contour, which is where the difference lives.
  const int aims[] = {72, 60, 79, 63, 84, 61, 75, 66, 81, 62, 77, 64};

  auto totalMotion = [&](const MelodyWeights &w) {
    MelodyState state;
    int total = 0;
    for (int aim : aims) {
      const auto idx = chooseNote(pool, ranks, ceiling, aim, state, w);
      if (state.lastNote >= 0)
        total += std::abs(pool[idx] - state.lastNote);
      state.advance(pool[idx]);
    }
    return total;
  };

  const int loose = totalMotion(MelodyWeights{1, 0, 0});
  const int smooth = totalMotion(MelodyWeights{1, 2, 0});
  CHECK_MSG(smooth < loose, "smoothing weight did not reduce total motion: " +
                                std::to_string(smooth) + " vs " +
                                std::to_string(loose));
}

TEST_CASE("ties resolve downward, so a seed reproduces a line") {
  // Two candidates equidistant from the aim with equal rank and equal interval
  // cost. The lower must win, every time, or the generator is not reproducible.
  const std::vector<int> pool{60, 64};
  const std::vector<int> ranks{0, 0};
  for (int i = 0; i < 8; ++i)
    CHECK_MSG(chooseNote(pool, ranks, 1000, 62, MelodyState{-1, 0}) == 0, "tie did not fall low");
}

TEST_CASE("an empty pool is survivable") {
  CHECK_MSG(chooseNote({}, {}, 1000, 72, MelodyState{60, 0}) == 0, "empty pool was not handled");
}

// ===========================================================================
// Direction. The second memory, and the one that decides whether a line reads
// as phrasing or as wandering.
// ===========================================================================

TEST_CASE("direction cost captures both rules at once") {
  // A run reads as intentional: having stepped up, stepping up again is free
  // and turning round is not.
  CHECK_MSG(directionCost(+2, +2) < directionCost(-2, +2), "a run should be free");
  CHECK_MSG(directionCost(-2, -2) < directionCost(+2, -2), "downward too");

  // A leap wants filling in: having jumped up, coming back is free and jumping
  // further is not. This is the OPPOSITE preference, which is the whole point.
  CHECK_MSG(directionCost(-2, +7) < directionCost(+2, +7), "a leap wants filling");
  CHECK_MSG(directionCost(+2, -12) < directionCost(-2, -12), "downward too");
}

TEST_CASE("the regimes switch at a fourth") {
  for (int last = 1; last <= 4; ++last) {
    CHECK_MSG(directionCost(+1, last) == 0, "continuing a step is free");
    CHECK_MSG(directionCost(-1, last) > 0, "reversing a step costs");
  }
  for (int last = 5; last <= 24; ++last) {
    CHECK_MSG(directionCost(-1, last) == 0, "filling a leap is free");
    CHECK_MSG(directionCost(+1, last) > 0, "extending a leap costs");
  }
}

TEST_CASE("direction cost is never negative") {
  // Gap-fill as a BONUS gives the same ordering and makes the objective's
  // scale depend on the history. Pinned so it stays a cost.
  for (int move = -24; move <= 24; ++move)
    for (int last = -24; last <= 24; ++last)
      CHECK_MSG(directionCost(move, last) >= 0,
                "negative at move " + std::to_string(move) + " last " +
                    std::to_string(last));
}

TEST_CASE("a line with no history pays no direction cost") {
  for (int move = -24; move <= 24; ++move)
    CHECK_MSG(directionCost(move, 0) == 0, "cost before the line has moved");
  for (int last = -24; last <= 24; ++last)
    CHECK_MSG(directionCost(0, last) == 0, "a repeated note has no direction");
}

TEST_CASE("MelodyState keeps the direction across a repeated note") {
  MelodyState st;
  st.advance(60);
  CHECK_MSG(st.lastMove == 0, "no move yet");
  st.advance(64);
  CHECK_MSG(st.lastMove == 4, "moved up a third");
  st.advance(64);
  CHECK_MSG(st.lastMove == 4, "a repeated note must not erase the direction");
  CHECK_MSG(st.lastNote == 64, "but it is still the last note");
  st.advance(62);
  CHECK_MSG(st.lastMove == -2, "turned round");
}

TEST_CASE("the direction weight lengthens runs") {
  // Same jagged aim sequence as the smoothing test, so the only difference is
  // the weight. Counted as how often the line keeps going the way it was.
  const auto pool = scalePool(cIonian, 4, 6);
  const std::vector<int> open(pool.size(), 0);
  const int aims[] = {70, 72, 74, 71, 76, 73, 78, 75, 80, 77, 74, 79,
                      72, 77, 70, 75, 68, 73, 66, 71};

  auto kept = [&](const MelodyWeights &w) {
    MelodyState state;
    int same = 0, turns = 0;
    for (int aim : aims) {
      const auto idx = chooseNote(pool, open, 1000, aim, state, w);
      const int move = pool[idx] - state.lastNote;
      if (state.lastNote >= 0 && move != 0 && state.lastMove != 0) {
        if ((move > 0) == (state.lastMove > 0))
          ++same;
        else
          ++turns;
      }
      state.advance(pool[idx]);
    }
    return turns > 0 ? (double)same / (same + turns) : 1.0;
  };

  const double off = kept(MelodyWeights{1, 2, 0});
  const double on = kept(MelodyWeights{1, 2, 3});
  CHECK_MSG(on > off, "the direction weight did not lengthen runs: " +
                          std::to_string(on) + " vs " + std::to_string(off));
}

TEST_CASE("the gate still holds with direction switched on") {
  // The regression that matters, re-asserted with the third term in play: a
  // new objective term is exactly where an admissibility check gets lost.
  const auto pool = scalePool(cIonian, 4, 6);
  const auto chord = chordOf(0, {0, 4, 7});
  const auto ranks = ranksFor(cIonian, pool, chord);
  const MelodyWeights loud{1, 2, 8};

  for (int strength = 0; strength <= 4; ++strength) {
    const int ceiling = rankCeiling(strength, /*hasChart=*/true);
    for (int aim = 48; aim <= 96; ++aim)
      for (int last = 48; last <= 96; last += 6)
        for (int move : {-12, -3, 0, 3, 12}) {
          const auto idx =
              chooseNote(pool, ranks, ceiling, aim, MelodyState{last, move}, loud);
          CHECK_MSG(ranks[idx] <= ceiling,
                    "direction weight broke the gate at strength " +
                        std::to_string(strength));
        }
  }
}

TEST_CASE("direction stays a tie-breaker, never a reason to leap") {
  // The constraint that keeps the two terms from cancelling. At half the
  // interval weight or below, raising direction must not make the line leap
  // more -- which is the failure measured on the real generator at 2 and 3.
  const auto pool = scalePool(cIonian, 4, 6);
  const std::vector<int> open(pool.size(), 0);

  // An aim sequence that swings hard, so gap-fill and contour can align.
  std::vector<int> aims;
  for (int i = 0; i < 60; ++i)
    aims.push_back(72 + ((i * 37) % 25) - 12);

  auto wideMoves = [&](int direction) {
    MelodyState state;
    int wide = 0;
    for (int aim : aims) {
      const auto idx =
          chooseNote(pool, open, 1000, aim, state, MelodyWeights{1, 4, direction});
      if (state.lastNote >= 0 && std::abs(pool[idx] - state.lastNote) >= 8)
        ++wide;
      state.advance(pool[idx]);
    }
    return wide;
  };

  const int base = wideMoves(0);
  CHECK_MSG(wideMoves(safeDirectionWeight(4)) <= base,
            "the safe direction weight made the line leap more");
}

TEST_CASE("the safe direction weight is half the interval weight") {
  CHECK_MSG(safeDirectionWeight(0) == 0, "no interval term, no direction term");
  for (int w = 1; w <= 20; ++w)
    CHECK_MSG(safeDirectionWeight(w) * 2 <= w, "direction outweighs interval at " +
                                                   std::to_string(w));
}
