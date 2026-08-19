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
  for (int d = -2; d <= 2; ++d)
    CHECK_MSG(intervalCost(d) == 0, "step of " + std::to_string(d) + " is not free");
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

TEST_CASE("no interval is ever cheaper than a step") {
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
        const auto idx = chooseNote(pool, ranks, ceiling, aim, last);
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
  CHECK_MSG(chooseNote(pool, ranks, 10, 64, -1) == 2, "fallback ignored the aim");
}

// ===========================================================================
// The objective itself.
// ===========================================================================

TEST_CASE("with no previous note this is pure contour following") {
  const auto pool = scalePool(cIonian, 4, 6);
  const std::vector<int> open(pool.size(), 0);
  for (int aim = 50; aim <= 94; ++aim) {
    const auto idx = chooseNote(pool, open, 1000, aim, /*lastNote=*/-1);
    for (std::size_t i = 0; i < pool.size(); ++i)
      CHECK_MSG(std::abs(pool[idx] - aim) <= std::abs(pool[i] - aim),
                "not nearest to aim " + std::to_string(aim));
  }
}

TEST_CASE("a weight of zero degrades to the old nearest-to-contour behaviour") {
  const auto pool = scalePool(cIonian, 4, 6);
  const std::vector<int> open(pool.size(), 0);
  const MelodyWeights contourOnly{1, 0};
  for (int aim = 50; aim <= 94; ++aim)
    for (int last = 48; last <= 96; last += 5) {
      const auto idx = chooseNote(pool, open, 1000, aim, last, contourOnly);
      const auto none = chooseNote(pool, open, 1000, aim, -1);
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
  const auto fromBelow = chooseNote(pool, open, 1000, aim, 62);
  const auto fromAbove = chooseNote(pool, open, 1000, aim, 84);
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
    int last = -1, total = 0;
    for (int aim : aims) {
      const auto idx = chooseNote(pool, ranks, ceiling, aim, last, w);
      if (last >= 0)
        total += std::abs(pool[idx] - last);
      last = pool[idx];
    }
    return total;
  };

  const int loose = totalMotion(MelodyWeights{1, 0});
  const int smooth = totalMotion(MelodyWeights{1, 2});
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
    CHECK_MSG(chooseNote(pool, ranks, 1000, 62, -1) == 0, "tie did not fall low");
}

TEST_CASE("an empty pool is survivable") {
  CHECK_MSG(chooseNote({}, {}, 1000, 72, 60) == 0, "empty pool was not handled");
}
