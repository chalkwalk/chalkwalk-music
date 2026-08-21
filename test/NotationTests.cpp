// SPDX-License-Identifier: MIT
#include "UnitTestCompat.h"
#include <chalkwalk/music/Notation.h>
#include <chalkwalk/music/Text.h>
#include <string>

using namespace chalkwalk::music;



namespace {

using namespace chalkwalk::music::Notation;


TEST_CASE("Notation -- keys as they are written and read") {
    SECTION("the shorthand people actually type")
    {
      // What gets typed in a jam is "Dm", not "D minor".
      const auto dm = parseName("Dm");
      EXPECT(dm.valid);
      EXPECT_EQ(displayName(dm), std::string("D minor"));

      const auto d = parseName("D");
      EXPECT(d.valid);
      EXPECT_EQ(displayName(d), std::string("D major"),
                   "a bare tonic means major");

      EXPECT_EQ(displayName(parseName("Bb")), std::string("Bb major"));
      EXPECT_EQ(displayName(parseName("Bbm")), std::string("Bb minor"));
      EXPECT_EQ(displayName(parseName("F#")), std::string("F# major"));
    }

    SECTION("a flat in second position is never a mode")
    {
      // "b" is the one character that could be either an accidental or the
      // start of a mode name. No mode begins with it, so the reading is
      // unambiguous -- but "Bm" must still be B minor, not B flat anything.
      const auto bFlat = parseName("Bb");
      const auto bMinor = parseName("Bm");
      EXPECT(bFlat.valid && bMinor.valid);
      EXPECT_EQ(displayName(bFlat), std::string("Bb major"));
      EXPECT_EQ(displayName(bMinor), std::string("B minor"));
      EXPECT(bFlat.tonic != bMinor.tonic, "Bb and B are different tonics");
    }

    SECTION("every mode round-trips through its own name")
    {
      for (const auto *name : {"major", "minor", "Ionian", "Dorian", "Phrygian",
                               "Lydian", "Mixolydian", "Aeolian", "Locrian"}) {
        const std::string spelled = std::string("D ") + name;
        const auto key = parseName(spelled);
        EXPECT(key.valid, "did not parse: " + spelled);
        EXPECT_EQ(displayName(key), spelled,
                     "did not round-trip: " + spelled);
      }
    }

    SECTION("minor and Aeolian stay distinct even though they are the same scale")
    {
      // Someone who typed "D minor" should be told "D minor" back. The scales
      // are identical; the words are not.
      EXPECT_EQ(displayName(parseName("D minor")), std::string("D minor"));
      EXPECT_EQ(displayName(parseName("D Aeolian")),
                   std::string("D Aeolian"));
      EXPECT_EQ(scaleNotes(parseName("D minor")),
                   scaleNotes(parseName("D Aeolian")),
                   "the notes must be the same even if the names are not");
    }

    SECTION("case and spacing do not matter")
    {
      for (const auto *s : {"dm", "DM", "D m", " Dm ", "d minor", "D MINOR"})
        EXPECT_EQ(displayName(parseName(s)), std::string("D minor"),
                     std::string("failed on: ") + s);
    }

    SECTION("the scale is spelled to match the tonic")
    {
      EXPECT_EQ(scaleNotes(parseName("D minor")),
                   std::string("D E F G A Bb C"));
      EXPECT_EQ(scaleNotes(parseName("C major")),
                   std::string("C D E F G A B"));
      // A mode is not just a relabelled major scale: F Dorian has four flats.
      EXPECT_EQ(scaleNotes(parseName("F Dorian")),
                   std::string("F G Ab Bb C D Eb"));
    }

    SECTION("prose is never a key")
    {
      // The whole reason the tagged form exists. Jamtaba's chord parser reads
      // "I AM TIRED ..." as a progression because it treats I and l as measure
      // separators -- that is in their own test suite. Guessing at prose gives
      // you a header that lies.
      for (const auto *s : {"I AM TIRED ...", "LETS TAKE A BREAK", "hello", "",
                            "   ", "H minor", "D quantum", "8", "Dmm"})
        EXPECT(!parseName(s).valid,
               std::string("wrongly read as a key: ") + s);
    }

}

} // namespace
