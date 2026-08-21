// SPDX-License-Identifier: MIT
#include "UnitTestCompat.h"
#include <chalkwalk/music/Harmony.h>
#include <chalkwalk/music/Notation.h>
#include <string>

using namespace chalkwalk::music;


// What a chat line does to the room's key and chart, in ONE place.
//
// It was two: `PracticeBot` learned to read degree charts and to move a chart
// through a key change, and the editor did neither -- so the band followed
// `| ii | V | I |` while the chord row above the phase bar went on showing the
// chart before it, and a key change transposed what you heard and not what you
// read. Two paths that must agree and had no reason to (`PRINCIPLES` 8).

namespace {

Notation::Key keyOf(const char *name) {
  auto k = Notation::parseName(name);
  REQUIRE(k.valid);
  return k;
}


TEST_CASE("Harmony -- a session's key and chart move together") {
    SECTION("a chart somebody typed survives a key change, transposed")
    {
      Harmony::Session st;
      st.key = keyOf("C major");
      EXPECT_EQ((int)Harmony::applyChart("| Am | F | C | G |", st),
                   (int)Harmony::Applied::Chart);
      EXPECT(st.chartFromChat, "a chart from chat was not recorded as one");

      EXPECT_EQ((int)Harmony::applyKey("D major", st),
                   (int)Harmony::Applied::Key);
      EXPECT_EQ(Harmony::chartText(st.chart, st.key),
                   std::string("| Bm | G | D | A |"),
                   "the chart did not travel with the key");
    }

    SECTION("a chart the key implied is rebuilt, not transposed")
    {
      // Nothing was written down, so there is nothing to preserve: the new key
      // gets its own default rather than the old key's default moved.
      Harmony::Session st;
      st.key = keyOf("C major");
      st.chart = Harmony::defaultChart(st.key);

      EXPECT_EQ((int)Harmony::applyKey("A minor", st),
                   (int)Harmony::Applied::Key);
      EXPECT_EQ(Harmony::chartText(st.chart, st.key),
                   Harmony::chartText(Harmony::defaultChart(keyOf("A minor")),
                                      keyOf("A minor")),
                   "a defaulted chart was moved instead of rebuilt");
    }

    SECTION("degrees are read against the key the room is in")
    {
      Harmony::Session st;
      st.key = keyOf("C major");
      EXPECT_EQ((int)Harmony::applyChart("| ii | V | I |", st),
                   (int)Harmony::Applied::Chart);
      EXPECT_EQ(Harmony::chartText(st.chart, st.key),
                   std::string("| Dm | G | C |"));
      EXPECT(st.chartFromChat, "a degree chart is still a chart somebody wrote");

      // ...and they mean something else in another key, which is the point.
      Harmony::Session minor;
      minor.key = keyOf("A minor");
      EXPECT_EQ((int)Harmony::applyChart("| ii | V | I |", minor),
                   (int)Harmony::Applied::Chart);
      EXPECT(Harmony::chartText(minor.chart, minor.key) !=
                 Harmony::chartText(st.chart, st.key),
             "degrees resolved to the same chords in two different keys");
    }

    SECTION("degrees need a key, and prose is never a chart")
    {
      Harmony::Session none;
      EXPECT_EQ((int)Harmony::applyChart("| ii | V | I |", none),
                   (int)Harmony::Applied::Nothing,
                   "degrees were resolved against no key at all");

      Harmony::Session st;
      st.key = keyOf("C major");
      for (const char *prose :
           {"I AM TIRED", "what are the chords", "sounds good", "",
            "| not | a | chart |"})
        EXPECT_EQ((int)Harmony::applyChart(prose, st),
                     (int)Harmony::Applied::Nothing,
                     std::string(prose) + " was taken for a chart");
    }

    SECTION("announcing the key twice changes nothing the second time")
    {
      Harmony::Session st;
      st.key = keyOf("C major");
      EXPECT_EQ((int)Harmony::applyChart("| Am | F |", st),
                   (int)Harmony::Applied::Chart);
      const auto before = Harmony::chartText(st.chart, st.key);

      EXPECT_EQ((int)Harmony::applyKey("D minor", st),
                   (int)Harmony::Applied::Key);
      const auto moved = Harmony::chartText(st.chart, st.key);
      EXPECT(moved != before, "the key change did nothing at all");

      // The same key again is not a change, and must not transpose twice --
      // which is the bug this shape of state is easiest to write.
      EXPECT_EQ((int)Harmony::applyKey("D minor", st),
                   (int)Harmony::Applied::Nothing);
      EXPECT_EQ(Harmony::chartText(st.chart, st.key), moved,
                   "re-announcing the key transposed the chart again");
    }
}

} // namespace
