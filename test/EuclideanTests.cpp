// SPDX-License-Identifier: MIT
#include <catch2/catch_test_macros.hpp>

#include <chalkwalk/music/Euclidean.h>

#include <numeric>
#include <string>
#include <vector>

// The merged suite. Three projects grew Euclidean generators independently and
// each grew its own tests; this is the union, plus the contract assertions that
// none of them had because none of them had a reason to state a cross-project
// property.

namespace m = chalkwalk::music;

namespace {

std::string render(const std::vector<bool> &p) {
  std::string s;
  s.reserve(p.size());
  for (bool b : p)
    s += b ? 'x' : '.';
  return s;
}

int countOnsets(const std::vector<bool> &p) {
  int n = 0;
  for (bool b : p)
    if (b)
      ++n;
  return n;
}

// The formulation this library chose AGAINST: textbook Bresenham with the error
// term seeded at steps/2, which centres each onset in its bin. Kept here so the
// relationship between the two can be asserted rather than asserted about.
std::vector<bool> centredFormulation(int steps, int beats) {
  if (steps <= 0)
    return {};
  if (beats <= 0)
    return std::vector<bool>(static_cast<std::size_t>(steps), false);
  if (beats >= steps)
    return std::vector<bool>(static_cast<std::size_t>(steps), true);

  std::vector<bool> out;
  int error = steps / 2;
  for (int i = 0; i < steps; ++i) {
    error -= beats;
    if (error < 0) {
      out.push_back(true);
      error += steps;
    } else {
      out.push_back(false);
    }
  }
  return out;
}

} // namespace

// ===========================================================================
// The phase contract. The reason this library exists rather than three of them.
// ===========================================================================

TEST_CASE("every pattern starts on the downbeat", "[euclidean][contract]") {
  for (int steps = 1; steps <= 64; ++steps)
    for (int pulses = 1; pulses <= steps; ++pulses) {
      const auto p = m::pattern(steps, pulses);
      INFO("E(" << pulses << "," << steps << ") = " << render(p));
      REQUIRE(!p.empty());
      REQUIRE(p[0]);
    }
}

TEST_CASE("the pattern table", "[euclidean][contract]") {
  struct Case {
    int steps;
    int pulses;
    const char *expected;
  };
  const Case cases[] = {
      {4, 1, "x..."},
      {4, 2, "x.x."},
      {4, 3, "x.xx"},
      {8, 1, "x......."},
      {8, 2, "x...x..."},
      {8, 3, "x..x..x."},
      {8, 4, "x.x.x.x."},
      {8, 5, "x.x.xx.x"},
      {8, 7, "x.xxxxxx"},
      {12, 3, "x...x...x..."},
      {12, 4, "x..x..x..x.."},
      {12, 5, "x..x.x..x.x."},
      {16, 4, "x...x...x...x..."},
      {16, 5, "x...x..x..x..x.."},
      {16, 7, "x..x.x.x..x.x.x."},
      {16, 9, "x.x.x.x.xx.x.x.x"},
  };
  for (const auto &c : cases) {
    INFO("E(" << c.pulses << "," << c.steps << ")");
    REQUIRE(render(m::pattern(c.steps, c.pulses)) == c.expected);
  }
}

// The names are worth being exact about: one of them was wrong in two projects
// for years, behind a test that only counted onsets.
TEST_CASE("the named patterns are exactly what the docs claim",
          "[euclidean][contract]") {
  REQUIRE(render(m::pattern(8, 3)) == "x..x..x.");  // the tresillo, exactly

  // NOT the cinquillo -- a rotation of it. Documented, and reachable.
  REQUIRE(render(m::pattern(8, 5)) == "x.x.xx.x");
  REQUIRE(render(m::pattern(8, 5, 6)) == "x.xx.xx."); // the cinquillo itself

  REQUIRE(render(m::pattern(16, 4)) == "x...x...x...x..."); // four on the floor
}

// The relationship with the formulation this library rejected: always the same
// necklace, frequently a different rotation. If this ever fails, the two have
// stopped being rotations of each other and something is badly wrong.
TEST_CASE("the centred formulation is always a rotation of this one",
          "[euclidean][contract]") {
  int differing = 0;
  int compared = 0;

  for (int steps = 2; steps <= 48; ++steps)
    for (int pulses = 1; pulses < steps; ++pulses) {
      const auto ours = render(m::pattern(steps, pulses));
      const auto theirs = render(centredFormulation(steps, pulses));
      ++compared;
      if (ours == theirs)
        continue;
      ++differing;

      bool isRotation = false;
      for (int r = 0; r < steps && !isRotation; ++r) {
        const auto rot = ours.substr(static_cast<std::size_t>(steps - r)) +
                         ours.substr(0, static_cast<std::size_t>(steps - r));
        if (rot == theirs)
          isRotation = true;
      }
      INFO("E(" << pulses << "," << steps << "): " << ours << " vs " << theirs);
      REQUIRE(isRotation);
    }

  INFO(differing << " of " << compared << " differ by rotation");
  REQUIRE(differing > 0);
  REQUIRE(compared > 1000);
}

// ===========================================================================
// The rhythm itself.
// ===========================================================================

TEST_CASE("the pattern has exactly the onsets asked for", "[euclidean]") {
  for (int steps = 1; steps <= 32; ++steps)
    for (int pulses = 0; pulses <= steps; ++pulses) {
      INFO("E(" << pulses << "," << steps << ")");
      REQUIRE(countOnsets(m::pattern(steps, pulses)) == pulses);
    }
}

// The property that makes these musical rather than arbitrary: the onsets are
// as evenly spread as an integer grid allows, so no two gaps differ by more
// than one step.
TEST_CASE("onsets are as evenly spread as the length allows", "[euclidean]") {
  for (int steps = 2; steps <= 48; ++steps)
    for (int pulses = 1; pulses <= steps; ++pulses) {
      const auto p = m::pattern(steps, pulses);

      std::vector<int> onsets;
      for (int i = 0; i < steps; ++i)
        if (p[static_cast<std::size_t>(i)])
          onsets.push_back(i);
      if (onsets.size() < 2)
        continue;

      int smallest = steps;
      int largest = 0;
      for (std::size_t i = 0; i < onsets.size(); ++i) {
        const int next = (i + 1 < onsets.size()) ? onsets[i + 1]
                                                 : onsets[0] + steps;
        const int gap = next - onsets[i];
        smallest = std::min(smallest, gap);
        largest = std::max(largest, gap);
      }
      INFO("E(" << pulses << "," << steps << ") gaps " << smallest << ".."
                << largest);
      REQUIRE(largest - smallest <= 1);
    }
}

TEST_CASE("degenerate inputs produce something rather than crashing",
          "[euclidean]") {
  REQUIRE(m::pattern(0, 4).empty());
  REQUIRE(m::pattern(-8, 4).empty());
  REQUIRE(countOnsets(m::pattern(8, 0)) == 0);
  REQUIRE(m::pattern(8, 0).size() == 8u);
  REQUIRE(countOnsets(m::pattern(8, -3)) == 0);
  REQUIRE(countOnsets(m::pattern(8, 99)) == 8);  // clamped, not overflowed
  REQUIRE(m::pattern(1, 1).size() == 1u);
  REQUIRE(m::pattern(1, 1)[0]);
}

// ===========================================================================
// Rotation.
// ===========================================================================

TEST_CASE("rotation is a pure right shift", "[euclidean]") {
  for (int steps : {5, 8, 16}) {
    const auto base = m::pattern(steps, 3);
    for (int offset = 0; offset < steps; ++offset) {
      const auto rotated = m::pattern(steps, 3, offset);
      REQUIRE(static_cast<int>(rotated.size()) == steps);
      REQUIRE(countOnsets(rotated) == 3);
      for (int i = 0; i < steps; ++i) {
        const int src = ((i - offset) % steps + steps) % steps;
        INFO("steps " << steps << " offset " << offset << " index " << i);
        REQUIRE(rotated[static_cast<std::size_t>(i)] ==
                base[static_cast<std::size_t>(src)]);
      }
    }
  }
}

TEST_CASE("rotation wraps in both directions and beyond a full cycle",
          "[euclidean]") {
  const auto base = m::pattern(8, 3);
  REQUIRE(m::pattern(8, 3, 8) == base);
  REQUIRE(m::pattern(8, 3, -8) == base);
  REQUIRE(m::pattern(8, 3, 16) == base);
  REQUIRE(m::pattern(8, 3, 11) == m::pattern(8, 3, 3));
  REQUIRE(m::pattern(8, 3, -1) == m::pattern(8, 3, 7));
  REQUIRE(m::pattern(8, 3, -11) == m::pattern(8, 3, 5));
}

TEST_CASE("every rotation is reachable", "[euclidean][contract]") {
  // The escape hatch that makes the phase contract costless: a caller wanting
  // any other phase can have it.
  for (int steps : {8, 12, 16}) {
    for (int pulses = 1; pulses < steps; ++pulses) {
      const auto target = centredFormulation(steps, pulses);
      bool reachable = false;
      for (int offset = 0; offset < steps && !reachable; ++offset)
        if (m::pattern(steps, pulses, offset) == target)
          reachable = true;
      INFO("E(" << pulses << "," << steps << ")");
      REQUIRE(reachable);
    }
  }
}

// ===========================================================================
// hit(): the audio-thread form.
// ===========================================================================

TEST_CASE("hit and pattern cannot disagree", "[euclidean]") {
  for (int steps = 1; steps <= 32; ++steps)
    for (int pulses = 0; pulses <= steps; ++pulses)
      for (int offset = -3; offset <= 3; ++offset) {
        const auto p = m::pattern(steps, pulses, offset);
        for (int i = 0; i < steps; ++i) {
          INFO("E(" << pulses << "," << steps << ") offset " << offset
                    << " step " << i);
          REQUIRE(p[static_cast<std::size_t>(i)] ==
                  m::hit(i, steps, pulses, offset));
        }
      }
}

TEST_CASE("hit is stable outside the first cycle", "[euclidean]") {
  for (int cycle = -3; cycle <= 3; ++cycle)
    for (int i = 0; i < 8; ++i) {
      INFO("cycle " << cycle << " step " << i);
      REQUIRE(m::hit(i + cycle * 8, 8, 3) == m::hit(i, 8, 3));
    }
}

TEST_CASE("hit handles degenerate inputs", "[euclidean]") {
  REQUIRE_FALSE(m::hit(0, 0, 3));
  REQUIRE_FALSE(m::hit(0, -8, 3));
  REQUIRE_FALSE(m::hit(0, 8, 0));
  REQUIRE_FALSE(m::hit(0, 8, -1));
  REQUIRE(m::hit(3, 8, 8));
  REQUIRE(m::hit(3, 8, 99));
}

// ===========================================================================
// patternPeriod: does the figure move, or lock?
// ===========================================================================

TEST_CASE("patternPeriod matches the period the pattern actually has",
          "[euclidean][period]") {
  for (int steps = 1; steps <= 40; ++steps)
    for (int pulses = 1; pulses < steps; ++pulses) {
      const auto p = m::pattern(steps, pulses);
      const int claimed = m::patternPeriod(steps, pulses);

      // Find the true period by search: the smallest d dividing steps for
      // which shifting by d leaves the pattern unchanged.
      int actual = steps;
      for (int d = 1; d <= steps; ++d) {
        if (steps % d != 0)
          continue;
        bool invariant = true;
        for (int i = 0; i < steps && invariant; ++i)
          if (p[static_cast<std::size_t>(i)] !=
              p[static_cast<std::size_t>((i + d) % steps)])
            invariant = false;
        if (invariant) {
          actual = d;
          break;
        }
      }
      INFO("E(" << pulses << "," << steps << ") claimed " << claimed
                << " actual " << actual);
      REQUIRE(claimed == actual);
    }
}

TEST_CASE("a common factor repeats, and that is a choice not a fault",
          "[euclidean][period]") {
  REQUIRE(m::patternPeriod(32, 8) == 4);   // eight repetitions of x...
  REQUIRE(m::patternPeriod(32, 9) == 32);  // spans the whole bar
  REQUIRE(m::patternPeriod(16, 4) == 4);
  REQUIRE(m::patternPeriod(16, 5) == 16);
}

TEST_CASE("patternPeriod handles degenerate inputs", "[euclidean][period]") {
  REQUIRE(m::patternPeriod(0, 4) == 0);
  REQUIRE(m::patternPeriod(-8, 4) == 0);
  REQUIRE(m::patternPeriod(8, 0) == 1);
  REQUIRE(m::patternPeriod(8, 8) == 1);
  REQUIRE(m::patternPeriod(8, 99) == 1);
}

// ===========================================================================
// nearestCoprimePulses.
// ===========================================================================

TEST_CASE("nearestCoprimePulses spans the pattern and stays near",
          "[euclidean][coprime]") {
  for (int steps = 2; steps <= 48; ++steps)
    for (int wanted = 1; wanted < steps; ++wanted)
      for (bool preferAbove : {true, false}) {
        const int got = m::nearestCoprimePulses(steps, wanted, preferAbove);
        INFO("steps " << steps << " wanted " << wanted << " got " << got);
        REQUIRE(got >= 1);
        REQUIRE(got <= steps - 1);
        REQUIRE(m::patternPeriod(steps, got) == steps);

        // Nothing strictly nearer is also coprime.
        const int distance = std::abs(got - wanted);
        for (int d = 1; d < distance; ++d) {
          if (wanted + d <= steps - 1)
            REQUIRE(m::patternPeriod(steps, wanted + d) != steps);
          if (wanted - d >= 1)
            REQUIRE(m::patternPeriod(steps, wanted - d) != steps);
        }
      }
}

TEST_CASE("an already-coprime count is left alone", "[euclidean][coprime]") {
  REQUIRE(m::nearestCoprimePulses(32, 9, true) == 9);
  REQUIRE(m::nearestCoprimePulses(32, 9, false) == 9);
  REQUIRE(m::nearestCoprimePulses(16, 5, true) == 5);
}

TEST_CASE("the tie-break moves the way it is asked to",
          "[euclidean][coprime]") {
  // 16 with 8 wanted: 7 and 9 are both coprime and equidistant.
  REQUIRE(m::nearestCoprimePulses(16, 8, true) == 9);
  REQUIRE(m::nearestCoprimePulses(16, 8, false) == 7);
}

TEST_CASE("degenerate step counts do not hang", "[euclidean][coprime]") {
  REQUIRE(m::nearestCoprimePulses(0, 3, true) == 0);
  REQUIRE(m::nearestCoprimePulses(1, 3, true) == 1);
  REQUIRE(m::nearestCoprimePulses(8, -5, true) >= 1);
  REQUIRE(m::nearestCoprimePulses(8, 999, true) <= 7);
}

// ===========================================================================
// accents.
// ===========================================================================

TEST_CASE("accents fall on onsets and nowhere else", "[euclidean][accents]") {
  for (int steps : {8, 12, 16})
    for (int pulses = 1; pulses < steps; ++pulses)
      for (int numAccents = 0; numAccents <= pulses; ++numAccents) {
        const auto p = m::pattern(steps, pulses);
        const auto v = m::accents(steps, pulses, 0, numAccents);
        REQUIRE(v.size() == p.size());
        for (int i = 0; i < steps; ++i) {
          const bool onset = p[static_cast<std::size_t>(i)];
          const int vel = v[static_cast<std::size_t>(i)];
          INFO("E(" << pulses << "," << steps << ") accents " << numAccents
                    << " step " << i);
          if (!onset)
            REQUIRE(vel == 0);
          else
            REQUIRE((vel == m::kOnsetVelocity || vel == m::kAccentedVelocity));
        }
      }
}

TEST_CASE("the accent count is what was asked for", "[euclidean][accents]") {
  for (int numAccents = 0; numAccents <= 5; ++numAccents) {
    const auto v = m::accents(16, 5, 0, numAccents);
    int accented = 0;
    for (int x : v)
      if (x == m::kAccentedVelocity)
        ++accented;
    INFO("asked for " << numAccents);
    REQUIRE(accented == numAccents);
  }
}

TEST_CASE("asking for no accents still velocities the onsets",
          "[euclidean][accents]") {
  const auto v = m::accents(8, 3, 0, 0);
  int onsets = 0;
  for (int x : v) {
    REQUIRE(x != m::kAccentedVelocity);
    if (x == m::kOnsetVelocity)
      ++onsets;
  }
  REQUIRE(onsets == 3);
}

TEST_CASE("more accents than onsets is clamped, not overflowed",
          "[euclidean][accents]") {
  const auto v = m::accents(8, 3, 0, 99);
  int accented = 0;
  for (int x : v)
    if (x == m::kAccentedVelocity)
      ++accented;
  REQUIRE(accented == 3);
}

TEST_CASE("no onsets means no velocities", "[euclidean][accents]") {
  const auto v = m::accents(8, 0, 0, 4);
  REQUIRE(v.size() == 8u);
  REQUIRE(std::accumulate(v.begin(), v.end(), 0) == 0);
}

TEST_CASE("accents follow the rotation", "[euclidean][accents]") {
  // The velocities must line up with the rotated pattern, not the unrotated
  // one -- an easy thing to get wrong when accents are computed separately.
  for (int offset : {0, 1, 3, 7, -2}) {
    const auto p = m::pattern(8, 5, offset);
    const auto v = m::accents(8, 5, offset, 2);
    for (int i = 0; i < 8; ++i) {
      INFO("offset " << offset << " step " << i);
      REQUIRE((v[static_cast<std::size_t>(i)] != 0) ==
              p[static_cast<std::size_t>(i)]);
    }
  }
}
