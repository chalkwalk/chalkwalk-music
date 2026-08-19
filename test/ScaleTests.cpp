// SPDX-License-Identifier: MIT
//
// The tonal core. Ported from Lockstep's suite with the assertions unchanged --
// this file is the acceptance test for that extraction.

#include <catch2/catch_test_macros.hpp>

#include <chalkwalk/music/Scale.h>

#include <algorithm>
#include <set>
#include <string>

// Lockstep's test harness took a message as CHECK's second argument. Rather
// than rewrite 108 assertions -- and risk changing what they assert while
// moving the code they cover -- the message becomes a Catch2 INFO. This file
// is the acceptance test for the extraction; rewriting it would defeat that.
// The condition is parenthesised because Catch2 refuses to decompose `&&` or
// a chained comparison, and several of these assertions use both. That costs
// the decomposed failure output, which the message more than replaces.
#define CHECK_MSG(cond, msg)                                                   \
    do {                                                                       \
        INFO(msg);                                                             \
        CHECK((cond));                                                         \
    } while (false)

namespace {

using namespace chalkwalk::music;

    // Helper: collect the absolute pitch classes of a key as a sorted set.
    static std::set<int> pcsOf(const KeySig& k)
    {
        std::set<int> out;
        const uint16_t m = pcMask(k);
        for (int pc = 0; pc < 12; ++pc)
            if (maskHas(m, pc)) out.insert(pc);
        return out;
    }

    TEST_CASE("brightness selects the mode, bright to dark", "[scale]")
    {
        // C root, the four corner modes by brightness.
        const std::set<int> ionian   { 0, 2, 4, 5, 7, 9, 11 };   // major
        const std::set<int> lydian   { 0, 2, 4, 6, 7, 9, 11 };   // brightest
        const std::set<int> aeolian  { 0, 2, 3, 5, 7, 8, 10 };   // natural minor
        const std::set<int> locrian  { 0, 1, 3, 5, 6, 8, 10 };   // darkest

        CHECK_MSG(pcsOf({ 0, kIonian,  {}, ScaleType::Diatonic }) == ionian,  "C Ionian pcs");
        CHECK_MSG(pcsOf({ 0, kLydian,  {}, ScaleType::Diatonic }) == lydian,  "C Lydian pcs");
        CHECK_MSG(pcsOf({ 0, kAeolian, {}, ScaleType::Diatonic }) == aeolian, "C Aeolian pcs");
        CHECK_MSG(pcsOf({ 0, kLocrian, {}, ScaleType::Diatonic }) == locrian, "C Locrian pcs");

        // degrees() returns intervals from root, first entry 0, ascending.
        const auto d = degrees({ 0, kIonian, {}, ScaleType::Diatonic });
        CHECK_MSG((d == std::vector<int>{ 0, 2, 4, 5, 7, 9, 11 }), "C Ionian degrees");
    }

    TEST_CASE("a modifier means the same thing in every relative mode", "[scale]")
    {
        // A Aeolian and C Ionian are relative modes — same note pool.
        const KeySig aMinor { 9, kAeolian, {}, ScaleType::Diatonic };
        const KeySig cMajor { 0, kIonian,  {}, ScaleType::Diatonic };
        CHECK_MSG(pcMask(aMinor) == pcMask(cMajor), "relative modes share the pool");

        // The Harmonic modifier (raise b7->7) is the SAME absolute operation on
        // the shared pool (G->G#), so the resulting pitch sets are identical...
        KeySig aHarm = aMinor; aHarm.modifiers = { NamedModifier::Harmonic };
        KeySig cHarm = cMajor; cHarm.modifiers = { NamedModifier::Harmonic };
        CHECK_MSG(pcMask(aHarm) == pcMask(cHarm), "Harmonic on relative modes -> same pcs");

        // ...but it reads as a different degree per root: leading-tone (7) in
        // the minor, raised-fifth (#5) in the relative major.
        CHECK_MSG(degreeNameOf(aHarm, NamedModifier::Harmonic) == "7",  "Harmonic reads '7' in minor");
        CHECK_MSG(degreeNameOf(cHarm, NamedModifier::Harmonic) == "#5", "Harmonic reads '#5' in major");

        // Blues add: same blue note either way, named b5 in minor / b3 in major.
        CHECK_MSG(degreeNameOf(aMinor, NamedModifier::Blues) == "b5", "Blues reads 'b5' in minor");
        CHECK_MSG(degreeNameOf(cMajor, NamedModifier::Blues) == "b3", "Blues reads 'b3' in major");
    }

    TEST_CASE("an Add introduces a note; an Alter moves one", "[scale]")
    {
        const KeySig cMajor { 0, kIonian, {}, ScaleType::Diatonic };
        CHECK_MSG(degrees(cMajor).size() == 7u, "diatonic has 7 notes");

        KeySig cBlues = cMajor; cBlues.modifiers = { NamedModifier::Blues };
        CHECK_MSG(degrees(cBlues).size() == 8u, "Add (Blues) grows the set by one");

        KeySig cHarm = cMajor; cHarm.modifiers = { NamedModifier::Harmonic };
        CHECK_MSG(degrees(cHarm).size() == 7u, "Alter (Harmonic) keeps the count");

        // The Add note is the only difference.
        auto base = pcsOf(cMajor), blues = pcsOf(cBlues);
        std::vector<int> added;
        std::set_difference(blues.begin(), blues.end(), base.begin(), base.end(),
                            std::back_inserter(added));
        CHECK_MSG((added == std::vector<int>{ 3 }), "C+Blues adds Eb (b3)");
    }

    TEST_CASE("a modifier is available only where it changes something", "[scale]")
    {
        // Plain diatonic: all the alterations apply at their home, and Blues
        // (an add of a note outside the pool) always applies.
        const KeySig cAeolian { 0, kAeolian, {}, ScaleType::Diatonic };
        CHECK_MSG(isCompatible(cAeolian, NamedModifier::Harmonic), "Harmonic ok in minor");
        CHECK_MSG(isCompatible(cAeolian, NamedModifier::Melodic),  "Melodic ok in minor");
        CHECK_MSG(isCompatible(cAeolian, NamedModifier::Blues),    "Blues ok in minor");

        // A second identical add is redundant -> incompatible.
        KeySig cBlues = cAeolian; cBlues.modifiers = { NamedModifier::Blues };
        CHECK_MSG(!isCompatible(cBlues, NamedModifier::Blues), "duplicate Blues add rejected");

        // At Mixolydian the Harmonic edge-target lands on the tonic — guarded.
        const KeySig cMixo { 0, kMixolydian, {}, ScaleType::Diatonic };
        CHECK_MSG(!isCompatible(cMixo, NamedModifier::Harmonic), "Harmonic rejected when it hits the root");

        // Harmonic at Phrygian yields Phrygian-dominant (raise b3->3): valid.
        const KeySig cPhryg { 0, kPhrygian, {}, ScaleType::Diatonic };
        CHECK_MSG(isCompatible(cPhryg, NamedModifier::Harmonic), "Harmonic ok in Phrygian");
        KeySig cPhrygDom = cPhryg; cPhrygDom.modifiers = { NamedModifier::Harmonic };
        CHECK_MSG((pcsOf(cPhrygDom) == std::set<int>{ 0, 1, 4, 5, 7, 8, 10 }), "Phrygian dominant set");

        // Symmetric scales sit outside the modifier system.
        const KeySig wt { 0, kIonian, {}, ScaleType::WholeTone };
        CHECK_MSG(!isCompatible(wt, NamedModifier::Harmonic), "no modifiers on symmetric scales");
    }

    TEST_CASE("the fifths window nests: triad inside pentatonic inside scale", "[scale]")
    {
        // The contiguous-fifths nesting of C major: pentatonic core (central 5)
        // = C D E G A; triad core (central 3) is a fifths arc, not the tonic
        // triad (DESIGN §39.11 note).
        const KeySig cMajor { 0, kIonian, {}, ScaleType::Diatonic };

        std::set<int> tier0, tier1;
        for (int pc = 0; pc < 12; ++pc)
        {
            const int t = coreTier(cMajor, pc);
            if (t == 0) tier0.insert(pc);
            if (t <= 1) tier1.insert(pc);
        }
        // Pentatonic core (tier <= 1): C D E G A.
        CHECK_MSG((tier1 == std::set<int>{ 0, 2, 4, 7, 9 }), "C major pentatonic core = C D E G A");
        // Triad core has exactly 3 notes and is a subset of the pentatonic core.
        CHECK_MSG(tier0.size() == 3u, "triad core has 3 notes");
        CHECK_MSG(std::includes(tier1.begin(), tier1.end(), tier0.begin(), tier0.end()),
              "triad core nests inside pentatonic core");

        // Out-of-scale notes are tier 3.
        CHECK_MSG(coreTier(cMajor, 1) == 3, "Db not in C major -> tier 3");
    }

    TEST_CASE("quantise snaps to the nearest scale tone", "[scale]")
    {
        const KeySig cMajor { 0, kIonian, {}, ScaleType::Diatonic };
        CHECK_MSG(quantize(cMajor, 60) == 60, "C4 in scale unchanged");
        CHECK_MSG(quantize(cMajor, 59) == 59, "B3 in scale unchanged");
        // In a gapless major every chromatic note is equidistant -> tie down.
        CHECK_MSG(quantize(cMajor, 61) == 60, "C#4 ties down to C4");
        CHECK_MSG(quantize(cMajor, 66) == 65, "F#4 ties down to F4");
        CHECK_MSG(quantize(cMajor, 70) == 69, "Bb4 ties down to A4");

        // Harmonic minor has a 3-semitone gap (F .. G#); G sits nearer G#.
        const KeySig aHarm { 9, kAeolian, { NamedModifier::Harmonic }, ScaleType::Diatonic };
        CHECK_MSG(quantize(aHarm, 67) == 68, "G4 snaps up to G#4 (nearer above) in A harmonic minor");
        CHECK_MSG(quantize(aHarm, 65) == 65, "F4 in scale unchanged");
    }

    TEST_CASE("scales carry the names a musician would use", "[scale]")
    {
        // 11.13: classicalName() names the ROOT too, e.g. "D Dorian" -- a mode
        // name with no root reads as a fact about no key in particular, which
        // is exactly the top-bar bug this fixes ("97 BPM 4/4 Dorian").
        CHECK_MSG(classicalName({ 0, kIonian,  {}, ScaleType::Diatonic }) == "C Ionian",  "plain mode name carries its root");
        CHECK_MSG(classicalName({ 9, kAeolian, {}, ScaleType::Diatonic }) == "A Aeolian", "minor mode name carries its root");

        // Same mode, different root -> different name (root is not baked into
        // the mode string, it is genuinely read from KeySig::root).
        CHECK_MSG(classicalName({ 2, kIonian, {}, ScaleType::Diatonic }) == "D Ionian",
              "the root actually varies, not a hard-coded prefix");

        KeySig aHarm { 9, kAeolian, { NamedModifier::Harmonic }, ScaleType::Diatonic };
        CHECK_MSG(classicalName(aHarm) == "A Harmonic minor", "Aeolian + Harmonic = Harmonic minor, rooted");

        KeySig aMel { 9, kAeolian, { NamedModifier::Melodic }, ScaleType::Diatonic };
        CHECK_MSG(classicalName(aMel) == "A Melodic minor", "Aeolian + Melodic = Melodic minor, rooted");

        CHECK_MSG(classicalName({ 0, kIonian, {}, ScaleType::WholeTone }) == "C Whole-tone", "whole-tone name, rooted");

        // An exotic combination with no textbook name returns empty -- an
        // unnamed root alone would read as a fragment, not a fix, so no root
        // is prefixed when there is no name to attach it to.
        KeySig exotic { 0, kLydian, { NamedModifier::Harmonic }, ScaleType::Diatonic };
        CHECK_MSG(classicalName(exotic).empty(), "unnamed exotic scale -> empty, not just a bare root");
    }

    TEST_CASE("the blue note reads correctly in every mode", "[scale]")
    {
        // The blue note is one fifths-anchored Add that reads as a different
        // degree per mode. As a full-scale (7-note) modifier it applies in ALL
        // seven modes, including Lydian (b7) and Locrian (b4).
        struct Case { int brightness; const char* degree; };
        const std::array<Case, 7> cases = {{
            { kLydian,     "b7" },
            { kIonian,     "b3" },
            { kMixolydian, "b6" },
            { kDorian,     "b2" },
            { kAeolian,    "b5" },
            { kPhrygian,   "b1" },
            { kLocrian,    "b4" },
        }};
        for (const auto& c : cases)
        {
            const KeySig k { 0, static_cast<int8_t>(c.brightness), {}, ScaleType::Diatonic };
            CHECK_MSG(isCompatible(k, NamedModifier::Blues),
                  std::string("Blues applies in ") + modeName(c.brightness));
            CHECK_MSG(degreeNameOf(k, NamedModifier::Blues) == c.degree,
                  std::string("Blues degree in ") + modeName(c.brightness) + " = " + c.degree);
        }
    }

    TEST_CASE("the blue note is gated by whether it fits the core", "[scale]")
    {
        // The pentatonic/triad restriction is a CORE-SIZE property: the blue note
        // fits a core only when the tonic is a member of that core.
        //   7 (full)      → all 7 modes
        //   5 (pentatonic)→ excludes Lydian, Locrian
        //   3 (triad)     → only Mixolydian, Dorian, Aeolian
        struct Case { int brightness; bool penta; bool triad; };
        const std::array<Case, 7> cases = {{
            { kLydian,     false, false },
            { kIonian,     true,  false },
            { kMixolydian, true,  true  },
            { kDorian,     true,  true  },
            { kAeolian,    true,  true  },
            { kPhrygian,   true,  false },
            { kLocrian,    false, false },
        }};
        for (const auto& c : cases)
        {
            const KeySig k { 0, static_cast<int8_t>(c.brightness), {}, ScaleType::Diatonic };
            CHECK_MSG(blueNoteFitsCore(k, 7), std::string("7-note blue note in ") + modeName(c.brightness));
            CHECK_MSG(blueNoteFitsCore(k, 5) == c.penta,
                  std::string("pentatonic blue note in ") + modeName(c.brightness));
            CHECK_MSG(blueNoteFitsCore(k, 3) == c.triad,
                  std::string("triad blue note in ") + modeName(c.brightness));
        }
    }

    TEST_CASE("the full modifier-by-mode availability matrix", "[scale]")
    {
        // Cross-mode availability of every modifier is INFERRED from the fifths
        // geometry, not enumerated by mode. A modifier is unavailable in a mode
        // for exactly one reason: one of its Raise/Lower ops would move the tonic
        // (target at fifths offset 0, i.e. brightness == -edgeOffset). Add (Blues)
        // never moves an existing note, so it is always available. This matrix is
        // what isCompatible derives; it is here to lock that the geometry — not a
        // table — is the source of truth. Order: Lydian..Locrian.
        const std::array<int, 7> brights =
            { kLydian, kIonian, kMixolydian, kDorian, kAeolian, kPhrygian, kLocrian };
        struct Row { NamedModifier mod; std::array<bool, 7> ok; };
        const std::array<Row, 6> rows = {{
            { NamedModifier::Harmonic,       {{ true,  true,  false, true,  true,  true,  true  }} },
            { NamedModifier::Melodic,        {{ false, true,  false, true,  true,  true,  true  }} },
            { NamedModifier::DoubleHarmonic, {{ true,  true,  false, false, true,  true,  true  }} },
            { NamedModifier::HarmonicMajor,  {{ true,  true,  true,  true,  false, true,  true  }} },
            { NamedModifier::Neapolitan,     {{ true,  true,  true,  false, true,  true,  true  }} },
            { NamedModifier::Blues,          {{ true,  true,  true,  true,  true,  true,  true  }} },
        }};
        for (const auto& r : rows)
            for (std::size_t i = 0; i < brights.size(); ++i)
            {
                const KeySig k { 0, static_cast<int8_t>(brights[i]), {}, ScaleType::Diatonic };
                CHECK_MSG(isCompatible(k, r.mod) == r.ok[i],
                      std::string(modifierName(r.mod)) + " in " + modeName(brights[i]));
            }
    }

    TEST_CASE("the default key is D Dorian", "[scale]")
    {
        const KeySig def;
        CHECK_MSG(def.root == 2, "default root is D");
        CHECK_MSG(def.brightness == kDorian, "default brightness is Dorian (symmetric centre)");
        // D Dorian = the white keys D E F G A B C.
        CHECK_MSG((pcsOf(def) == std::set<int>{ 2, 4, 5, 7, 9, 11, 0 }), "D Dorian = white keys");
    }

    TEST_CASE("a modifier that cannot apply lies dormant rather than corrupting the scale", "[scale]")
    {
        // Harmonic set in Aeolian, then the brightness moved to Mixolydian (where
        // Harmonic's op would hit the tonic): the modifier stays in the list but
        // is dormant — the scale is the plain mode, tonic intact.
        KeySig k { 0, kAeolian, { NamedModifier::Harmonic }, ScaleType::Diatonic };
        CHECK_MSG(isCompatible(KeySig{ 0, kAeolian, {}, ScaleType::Diatonic }, NamedModifier::Harmonic),
              "Harmonic applies in Aeolian");
        k.brightness = kMixolydian;   // now incompatible
        CHECK_MSG(maskHas(pcMask(k), 0), "dormant modifier keeps the tonic");
        CHECK_MSG(pcMask(k) == baseWindowMask(0, kMixolydian),
              "dormant modifier leaves the scale unaltered (plain Mixolydian)");
        CHECK_MSG(k.modifiers.size() == 1, "dormant modifier is retained in the list");
    }

    TEST_CASE("each scale type has the note count it claims", "[scale]")
    {
        // The scale type IS the note count: central-N fifths for 3/5/7, the two
        // symmetric scales at 6/8.
        CHECK_MSG((pcsOf({ 0, kIonian, {}, ScaleType::Diatonic }) == std::set<int>{ 0, 2, 4, 5, 7, 9, 11 }),
              "Diatonic = 7 notes");
        CHECK_MSG((pcsOf({ 0, kIonian, {}, ScaleType::Pentatonic }) == std::set<int>{ 0, 2, 4, 7, 9 }),
              "Pentatonic = central 5 = C D E G A");
        CHECK_MSG((pcsOf({ 0, kIonian, {}, ScaleType::Triad }) == std::set<int>{ 2, 7, 9 }),
              "Triad = central 3 fifths (D G A)");
        CHECK_MSG(pcsOf({ 0, kIonian, {}, ScaleType::WholeTone }).size() == 6u, "Whole-tone = 6 notes");
        CHECK_MSG(pcsOf({ 0, kIonian, {}, ScaleType::Diminished }).size() == 8u, "Diminished = 8 notes");
        CHECK_MSG(pcsOf({ 0, kIonian, {}, ScaleType::Chromatic }).size() == 12u, "Chromatic = all 12 notes");
        // Chromatic is the deliberate "no scale" — no modifiers, identity quantize.
        CHECK_MSG(!isCompatible({ 0, kIonian, {}, ScaleType::Chromatic }, NamedModifier::Harmonic),
              "no modifiers on Chromatic");
        CHECK_MSG(quantize({ 0, kIonian, {}, ScaleType::Chromatic }, 61) == 61, "Chromatic quantize is identity");
        CHECK_MSG(noteCountOf(ScaleType::Triad) == 3 && noteCountOf(ScaleType::Diminished) == 8,
              "note-count helper");

        // The blue note follows the chosen size: it fits C-Ionian pentatonic
        // (root in the pentatonic) but goes dormant in Lydian pentatonic.
        CHECK_MSG(pcsOf({ 0, kIonian, { NamedModifier::Blues }, ScaleType::Pentatonic }).size() == 6u,
              "pentatonic + blues = 6 (blue note fits)");
        CHECK_MSG(pcsOf({ 0, kLydian, { NamedModifier::Blues }, ScaleType::Pentatonic }).size() == 5u,
              "Lydian pentatonic + blues = 5 (blue note dormant)");
    }

    TEST_CASE("roots order around the circle of fifths", "[scale]")
    {
        // D is the centre (index 6); each step is a fifth.
        CHECK_MSG(rootPcAtFifthsIndex(6) == 2, "fifths index 6 = D (centre)");
        CHECK_MSG(rootPcAtFifthsIndex(7) == 9, "one sharp-ward = A");
        CHECK_MSG(rootPcAtFifthsIndex(5) == 7, "one flat-ward = G");
        CHECK_MSG(rootPcAtFifthsIndex(11) == 1, "sharp end = C#");
        CHECK_MSG(rootPcAtFifthsIndex(0) == 8, "flat end = Ab");
        // Round-trip every pitch class.
        for (int pc = 0; pc < 12; ++pc)
            CHECK_MSG(rootPcAtFifthsIndex(fifthsIndexOfRootPc(pc)) == pc,
                  std::string("fifths-root round-trips pc ") + std::to_string(pc));
    }

    TEST_CASE("a modifier applies wholly or not at all", "[scale]")
    {
        // Multi-op modifiers are all-or-nothing. At Mixolydian, Melodic's
        // raise-edge2 hits the tonic, so the WHOLE modifier is dormant — it must
        // not half-apply (the old bug showed it bright-green "7").
        const KeySig cMixo { 0, kMixolydian, {}, ScaleType::Diatonic };
        CHECK_MSG(!isCompatible(cMixo, NamedModifier::Melodic), "Melodic unavailable at Mixolydian");
        CHECK_MSG(!isCompatible(cMixo, NamedModifier::DoubleHarmonic), "Double-harmonic unavailable at Mixolydian");

        KeySig mixoMel = cMixo; mixoMel.modifiers = { NamedModifier::Melodic };
        CHECK_MSG(!modifierApplies(mixoMel, NamedModifier::Melodic), "Melodic is dormant at Mixolydian");
        CHECK_MSG(pcMask(mixoMel) == baseMask(cMixo), "dormant Melodic leaves the plain mode (no partial apply)");

        // Multi-op modifiers name every altered degree.
        const KeySig aMel { 9, kAeolian, { NamedModifier::Melodic }, ScaleType::Diatonic };
        CHECK_MSG(degreeNameOf(aMel, NamedModifier::Melodic) == "6 7", "Melodic reads '6 7'");
        const KeySig pDbl { 0, kPhrygian, { NamedModifier::DoubleHarmonic }, ScaleType::Diatonic };
        CHECK_MSG(degreeNameOf(pDbl, NamedModifier::DoubleHarmonic) == "3 7", "Double-harmonic reads '3 7'");
    }

    TEST_CASE("a selected modifier changes what else is available", "[scale]")
    {
        // Selecting Harmonic raises edge2, so the other edge2-raisers
        // (Melodic, Double-harmonic) become unavailable cumulatively.
        const KeySig cAeolian { 0, kAeolian, {}, ScaleType::Diatonic };
        CHECK_MSG(isCompatible(cAeolian, NamedModifier::Melodic), "Melodic available before Harmonic");
        KeySig withHarm = cAeolian; withHarm.modifiers = { NamedModifier::Harmonic };
        CHECK_MSG(!isCompatible(withHarm, NamedModifier::Melodic), "Melodic unavailable after Harmonic");
        CHECK_MSG(!isCompatible(withHarm, NamedModifier::DoubleHarmonic), "Double-harmonic unavailable after Harmonic");
        // Non-conflicting modifier (different edge) stays available.
        CHECK_MSG(isCompatible(withHarm, NamedModifier::Neapolitan), "Neapolitan still available with Harmonic");

        // Order independence: {Harmonic, Neapolitan} gives the same scale either way.
        KeySig ab { 0, kAeolian, { NamedModifier::Harmonic, NamedModifier::Neapolitan }, ScaleType::Diatonic };
        KeySig ba { 0, kAeolian, { NamedModifier::Neapolitan, NamedModifier::Harmonic }, ScaleType::Diatonic };
        CHECK_MSG(pcMask(ab) == pcMask(ba), "modifier set is order-independent");
    }

    TEST_CASE("brightness range follows the window size", "[scale]")
    {
        auto [d0, d1] = brightnessRange(ScaleType::Diatonic);
        CHECK_MSG(d0 == kLocrian && d1 == kLydian, "diatonic spans all 7 modes");
        auto [p0, p1] = brightnessRange(ScaleType::Pentatonic);
        CHECK_MSG(p0 == kPhrygian && p1 == kIonian, "pentatonic drops Lydian/Locrian");
        auto [t0, t1] = brightnessRange(ScaleType::Triad);
        CHECK_MSG(t0 == kAeolian && t1 == kMixolydian, "triad keeps Mixo/Dorian/Aeolian");
        // Reducing notes clamps an out-of-range mode.
        CHECK_MSG(clampBrightness(ScaleType::Pentatonic, kLydian) == kIonian, "Lydian clamps to Ionian for penta");
        CHECK_MSG(clampBrightness(ScaleType::Triad, kLocrian) == kAeolian, "Locrian clamps to Aeolian for triad");
    }

    TEST_CASE("modifier order has one definition", "[scale]")
    {
        // kModifierOrder is the single source shared by the apply precedence and
        // the editor button order: rank == index, every modifier appears once.
        std::set<int> seen;
        for (int i = 0; i < static_cast<int>(kModifierOrder.size()); ++i)
        {
            CHECK_MSG(modifierRank(kModifierOrder[static_cast<std::size_t>(i)]) == i,
                  "modifierRank matches kModifierOrder index");
            seen.insert(static_cast<int>(kModifierOrder[static_cast<std::size_t>(i)]));
        }
        CHECK_MSG(seen.size() == 6u, "all six modifiers present in the order, no duplicates");
    }

    TEST_CASE("scale mode: off, snap and filter", "[scale]")
    {
        // Per-track Scale stage: Off passes through; Filter drops out-of-key;
        // Snap conforms out-of-key to an in-key pitch and leaves in-key notes.
        const KeySig dDorian{ 2, kDorian, {}, ScaleType::Diatonic };
        const uint16_t mask = pcMask(dDorian);

        CHECK_MSG(applyScaleMode(dDorian, ScaleMode::Off, 61) == std::optional<int>(61),
              "Off passes through");

        CHECK_MSG(applyScaleMode(dDorian, ScaleMode::Filter, 62) == std::optional<int>(62),
              "Filter keeps in-key D");
        CHECK_MSG(applyScaleMode(dDorian, ScaleMode::Filter, 61) == std::nullopt,
              "Filter drops out-of-key C#");

        const auto snapped = applyScaleMode(dDorian, ScaleMode::Snap, 61);
        CHECK_MSG(snapped.has_value() && maskHas(mask, ((*snapped % 12) + 12) % 12),
              "Snap conforms C# into the key");
        CHECK_MSG(applyScaleMode(dDorian, ScaleMode::Snap, 62) == std::optional<int>(62),
              "Snap leaves an in-key note unchanged");
    }

    TEST_CASE("modifiers pack to bits and back", "[scale]")
    {
        // The modifier list round-trips through the 6-bit serialization mask.
        std::vector<NamedModifier> mods = { NamedModifier::Harmonic, NamedModifier::Blues };
        const uint8_t bits = packModifiers(mods);
        CHECK_MSG(bits == ((1u << 0) | (1u << 4)), "Harmonic+Blues pack to bits 0 and 4");
        const auto back = unpackModifiers(bits);
        CHECK_MSG((back == std::vector<NamedModifier>{ NamedModifier::Harmonic, NamedModifier::Blues }),
              "modifier set round-trips (ascending id order)");
        CHECK_MSG(packModifiers({}) == 0, "empty modifier set packs to 0");
        CHECK_MSG(unpackModifiers(0).empty(), "0 unpacks to empty set");
    }

    TEST_CASE("note strength ranks by fifths distance, with the brightness lean", "[scale]")
    {
        // Fifths-offset inverts the "fifth = 7 semitones" map. Root C (0):
        CHECK_MSG(fifthsOffsetOf(0, 0) == 0,  "C is 0 fifths from C");
        CHECK_MSG(fifthsOffsetOf(0, 7) == 1,  "G is +1 fifth from C");
        CHECK_MSG(fifthsOffsetOf(0, 5) == -1, "F is -1 fifth from C");
        CHECK_MSG(fifthsOffsetOf(0, 2) == 2,  "D is +2 fifths from C");
        CHECK_MSG(fifthsOffsetOf(0, 4) == 4,  "E is +4 fifths from C (the major 3rd is far)");

        // Strength: root strongest, then the nearest fifths, colour notes weakest.
        const KeySig dorian{ 2, kDorian, {}, ScaleType::Diatonic };  // balanced lean
        CHECK_MSG(noteStrengthRank(dorian, 2) == 0, "root is rank 0");
        CHECK_MSG(noteStrengthRank(dorian, 9) < noteStrengthRank(dorian, 4),
              "the fifth outranks a far (4-fifths) note");
        // The major 3rd sits 4 fifths out — weaker than the 4th/5th, the quartal
        // grain the whole system already uses (coreTier's central-3 = fifths arc).
        CHECK_MSG(noteStrengthRank({ 0, kIonian, {}, ScaleType::Diatonic }, 4)
                > noteStrengthRank({ 0, kIonian, {}, ScaleType::Diatonic }, 7),
              "in C Ionian the 3rd (E) ranks weaker than the 5th (G)");

        // Directional lean tie-break: a sharp-leaning scale favours the +offset
        // note over the equidistant -offset note.
        const KeySig sharp{ 0, kLydian, {}, ScaleType::Diatonic };  // lean = +3
        CHECK_MSG(noteStrengthRank(sharp, 7) < noteStrengthRank(sharp, 5),
              "sharp lean: +1 (G) beats -1 (F) at equal distance");
        const KeySig flat{ 0, kLocrian, {}, ScaleType::Diatonic };  // lean = -3
        CHECK_MSG(noteStrengthRank(flat, 5) < noteStrengthRank(flat, 7),
              "flat lean: -1 (F) beats +1 (G) at equal distance");
    }

}  // namespace
