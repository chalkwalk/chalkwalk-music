// SPDX-License-Identifier: MIT
//
// Scala tuning. Ported from Arps Euclidya's suite, assertions unchanged.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include <chalkwalk/music/Tuning.h>

#include <string>

using namespace chalkwalk::music;

// ──────────────────────────────────────────────────────────────────────────────
// parseSclCents — valid inputs
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("parses 12-TET cents lines", "[tuning]") {
  const std::string scl =
      "! 12tet.scl\n"
      "12 equal temperament\n"
      "12\n"
      "!\n"
      "100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n"
      "700.0\n800.0\n900.0\n1000.0\n1100.0\n1200.0\n";

  std::string name;
  auto cents = parseScl(scl, name);
  REQUIRE(cents.size() == 12u);
  CHECK_THAT(cents[0], Catch::Matchers::WithinAbs(100.0, 0.001));
  CHECK_THAT(cents[11], Catch::Matchers::WithinAbs(1200.0, 0.001));
}

TEST_CASE("parses ratio lines (3/2 = 701.955 cents)", "[tuning]") {
  const std::string scl =
      "! ratio.scl\n"
      "Test with ratios\n"
      "2\n"
      "!\n"
      "3/2\n"
      "2/1\n";

  std::string name;
  auto cents = parseScl(scl, name);
  REQUIRE(cents.size() == 2u);
  CHECK_THAT(cents[0], Catch::Matchers::WithinAbs(701.955, 0.01));
  CHECK_THAT(cents[1], Catch::Matchers::WithinAbs(1200.0, 0.01));
}

TEST_CASE("skips comment lines", "[tuning]") {
  const std::string scl =
      "! comments.scl\n"
      "Scale with comments\n"
      "3\n"
      "!\n"
      "! this is a comment\n"
      "200.0\n"
      "! another comment\n"
      "400.0\n"
      "600.0\n";

  std::string name;
  auto cents = parseScl(scl, name);
  REQUIRE(cents.size() == 3u);
  CHECK_THAT(cents[0], Catch::Matchers::WithinAbs(200.0, 0.001));
}

TEST_CASE("returns name from description line", "[tuning]") {
  const std::string scl =
      "! named.scl\n"
      "My Favourite Scale\n"
      "1\n"
      "100.0\n";

  std::string name;
  // The scale itself is checked elsewhere; this case is about the name line.
  (void)parseScl(scl, name);
  CHECK(name == "My Favourite Scale");
}

// ──────────────────────────────────────────────────────────────────────────────
// parseSclCents — malformed inputs don't crash
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("empty input returns empty", "[tuning]") {
  std::string name;
  auto cents = parseScl("", name);
  CHECK(cents.empty());
}

TEST_CASE("garbage input returns empty without crash", "[tuning]") {
  std::string name;
  auto cents =
      parseScl("not a scala file at all\n###\n", name);
  CHECK(cents.size() <= 128u);  // may return partial, but must not crash
}

// ──────────────────────────────────────────────────────────────────────────────
// parseKbm
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseKbm reads reference frequency", "[scala][kbm]") {
  const std::string kbm =
      "! default.kbm\n"
      "12\n"
      "0\n"
      "127\n"
      "60\n"
      "69\n"
      "432.0\n"
      "12\n"
      "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n";

  auto kbmData = parseKbm(kbm);
  CHECK_THAT(kbmData.refFreq, Catch::Matchers::WithinAbs(432.0, 0.001));
  CHECK(kbmData.refNote == 69);
  CHECK(kbmData.middleNote == 60);
}

TEST_CASE("parseKbm handles 'x' unmapped entries", "[scala][kbm]") {
  const std::string kbm =
      "!\n"
      "7\n"
      "0\n127\n60\n69\n440.0\n7\n"
      "0\nx\n1\nx\n2\nx\n3\n";

  auto kbmData = parseKbm(kbm);
  REQUIRE(kbmData.mapping.size() == 7u);
  CHECK(kbmData.mapping[1] == -1);  // 'x' mapped to -1
  CHECK(kbmData.mapping[3] == -1);
}

// ──────────────────────────────────────────────────────────────────────────────
// computeTable — 12-TET produces near-zero deviations
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("12-TET scale produces near-zero cents deviation", "[tuning]") {
  const std::string scl =
      "! 12tet.scl\n"
      "12 equal temperament\n"
      "12\n"
      "100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n"
      "700.0\n800.0\n900.0\n1000.0\n1100.0\n1200.0\n";

  std::string name;
  auto cents = parseScl(scl, name);
  auto kbm = defaultKbm((int)cents.size());
  auto table = computeTable(cents, kbm, name);

  CHECK(table.isIdentity());
}

// ---------------------------------------------------------------------------
// Coverage the original suite did not have. These are the paths where a
// tuning parser is most likely to be quietly wrong: the period is assumed to
// be an octave, keys below the middle note take the wrong branch of a floor
// division, and unmapped keys are read as degree zero rather than skipped.
// ---------------------------------------------------------------------------

TEST_CASE("the period is the last entry, not an assumed octave",
          "[tuning][period]") {
  // Bohlen-Pierce repeats at 3/1 (1901.955 cents), not at the octave. A
  // parser that hardcodes 1200 gets every note outside the first period wrong.
  const std::string scl =
      "! bp.scl\n"
      "Bohlen-Pierce (first three degrees)\n"
      "3\n"
      "9/7\n"
      "7/5\n"
      "3/1\n";

  std::string name;
  const auto cents = parseScl(scl, name);
  REQUIRE(cents.size() == 3u);
  CHECK_THAT(cents[2], Catch::Matchers::WithinAbs(1901.955, 0.01));

  const auto kbm = defaultKbm(static_cast<int>(cents.size()));
  const auto table = computeTable(cents, kbm, name);

  // Three keys up from middle is one full period, so it must be 3/1 above the
  // middle note -- 1901.955 cents, not 1200.
  const double middleCents = 1200.0 * std::log2(
      440.0 * std::pow(2.0, (60 - 69) / 12.0));
  const double upCents = 1200.0 * std::log2(
      440.0 * std::pow(2.0, (63 - 69) / 12.0));
  const double actual = (upCents + table.centsDeviation[63]) -
                        (middleCents + table.centsDeviation[60]);
  INFO("three keys up measured " << actual << " cents");
  CHECK_THAT(actual, Catch::Matchers::WithinAbs(1901.955, 0.05));
}

TEST_CASE("keys below the middle note tune correctly", "[tuning][floor]") {
  // C++ integer division truncates toward zero, so a naive `key / mapSize`
  // puts everything between middleNote-mapSize+1 and middleNote-1 in the wrong
  // period. Quarter-comma meantone makes the error obvious.
  const std::string scl =
      "! qcm.scl\n"
      "Quarter-comma meantone fifth\n"
      "2\n"
      "696.578\n"
      "1200.0\n";

  std::string name;
  const auto cents = parseScl(scl, name);
  const auto kbm = defaultKbm(static_cast<int>(cents.size()));
  const auto table = computeTable(cents, kbm, name);

  for (int n = 0; n < 128; ++n) {
    INFO("note " << n << " deviation " << table.centsDeviation[n]);
    CHECK(std::isfinite(table.centsDeviation[n]));
  }

  // One full period below the middle note must be exactly an octave down.
  const double below = (1200.0 * std::log2(440.0 * std::pow(2.0, (58 - 69) / 12.0))
                        + table.centsDeviation[58]);
  const double mid = (1200.0 * std::log2(440.0 * std::pow(2.0, (60 - 69) / 12.0))
                      + table.centsDeviation[60]);
  INFO("two keys below measured " << (mid - below) << " cents");
  CHECK_THAT(mid - below, Catch::Matchers::WithinAbs(1200.0, 0.05));
}

TEST_CASE("an unmapped key is left at 12-TET rather than snapped to degree 0",
          "[tuning][kbm]") {
  const std::string scl = "! s.scl\nTwo note\n2\n600.0\n1200.0\n";
  std::string name;
  const auto cents = parseScl(scl, name);

  KeyboardMapping kbm = defaultKbm(2);
  kbm.mapping[1] = -1;  // the second key of each period is unmapped

  const auto table = computeTable(cents, kbm, name);
  CHECK(table.centsDeviation[61] == 0.0f);
  CHECK(std::isfinite(table.centsDeviation[60]));
}

TEST_CASE("the reference note lands on its reference frequency",
          "[tuning][kbm]") {
  // Whatever the scale, the pinned key must come out at exactly refFreq --
  // this is what stops a tuning drifting the whole instrument sharp or flat.
  const std::string scl =
      "! p.scl\nPythagorean fifths\n3\n203.910\n701.955\n1200.0\n";
  std::string name;
  const auto cents = parseScl(scl, name);

  KeyboardMapping kbm = defaultKbm(static_cast<int>(cents.size()));
  kbm.refNote = 69;
  kbm.refFreq = 440.0;

  const auto table = computeTable(cents, kbm, name);
  INFO("A4 deviation " << table.centsDeviation[69]);
  CHECK_THAT(static_cast<double>(table.centsDeviation[69]),
             Catch::Matchers::WithinAbs(0.0, 0.001));

  // ...and moving the reference frequency moves the whole instrument with it.
  kbm.refFreq = 432.0;
  const auto flat = computeTable(cents, kbm, name);
  const double expected = 1200.0 * std::log2(432.0 / 440.0);
  INFO("A4 at 432 Hz deviation " << flat.centsDeviation[69]);
  CHECK_THAT(static_cast<double>(flat.centsDeviation[69]),
             Catch::Matchers::WithinAbs(expected, 0.01));
}

TEST_CASE("parseTuning does the whole job from text", "[tuning]") {
  const std::string scl =
      "! t.scl\n12 equal\n12\n"
      "100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n"
      "700.0\n800.0\n900.0\n1000.0\n1100.0\n1200.0\n";
  const auto table = parseTuning(scl);
  CHECK(table.isIdentity());
  CHECK(table.stepsPerOctave == 12);
  CHECK(table.name == "12 equal");
}

TEST_CASE("a scale with no description falls back to the name given",
          "[tuning]") {
  const std::string scl = "!\n\n2\n600.0\n1200.0\n";
  const auto table = parseTuning(scl, {}, "from-the-filename");
  CHECK(table.name == "from-the-filename");
}
