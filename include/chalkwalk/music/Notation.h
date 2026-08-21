#pragma once

#include "Text.h"
#include <string>

// A key as it is WRITTEN and READ: a tonic, a mode, and how to spell it.
//
// The notational half of a key, and the counterpart to `Scale.h`. `KeySig`
// models what a scale IS -- a window on the circle of fifths, with modifiers,
// for generating notes. This models what a musician writes down: "D minor",
// spelled with a B flat rather than an A sharp, parsed from what somebody
// typed and displayed back to them.
//
// The two overlap and should eventually be one. `Key{tonic, Mode}` is the
// diatonic, no-modifier special case of `KeySig`, and `toKeySig` below is the
// bridge -- it maps the seven modes onto `brightness` one for one, because
// brightness IS the mode. What `KeySig` has no room for is the SPELLING, which
// is not a property of the scale at all: A major and B double-flat major are
// the same seven pitches and different documents. Merging them means deciding
// where spelling lives, and that is a decision rather than a refactor.
//
// See ROADMAP.md.

namespace chalkwalk::music::Notation {

// The seven diatonic modes plus the two everyone actually says. Major and Minor
// are kept distinct from Ionian and Aeolian even though they are the same
// scale: someone who typed "D minor" should see "D minor" back, not "D Aeolian".
enum class Mode {
  Major,
  Minor,
  Ionian,
  Dorian,
  Phrygian,
  Lydian,
  Mixolydian,
  Aeolian,
  Locrian
};

struct Key {
  bool valid = false;
  int tonic = 0;     // semitones above C, 0-11
  bool flat = false; // spell the tonic with a flat rather than a sharp
  Mode mode = Mode::Major;

  bool operator==(const Key &o) const {
    return valid == o.valid && tonic == o.tonic && mode == o.mode;
  }
  bool operator!=(const Key &o) const { return !(*this == o); }
};


// "D minor", "F# Dorian", "Bb major". Returns an invalid Key for anything else.
Key parseName(const std::string &text);




// "D minor". Empty for an invalid key.
std::string displayName(const Key &key);


// The notes of the scale, spelled to match the tonic: "D E F G A Bb C".
// Empty for an invalid key. Useful spoken as well as shown -- a player who
// cannot see the header still gets the one fact they need.
std::string scaleNotes(const Key &key);

std::string modeName(Mode mode);

// A pitch class as a note name, spelled sharp or flat as asked: "C#" or "Db".
//
// Exported because spelling a chord root is the same problem as spelling a
// scale note, and a second accidental table in Harmony.cpp would be a second
// place to be wrong (`PRINCIPLES §8`).
std::string noteName(int semitone, bool flat);

// Whether a key is conventionally written with flats, derived from its relative
// major. What `scaleNotes` uses, and what a chord name should use, so a chord in
// D minor spells Bb rather than A#.
bool usesFlats(int tonic, Mode mode);

// The seven scale degrees as semitones above the tonic, for anything that has
// to make a note rather than name one. `scaleNotes` spells them for a reader;
// this is the same information for a synthesiser.
//
// Always seven entries, and always the mode's own steps -- Major and Ionian
// coincide here, as do Minor and Aeolian, because the distinction between them
// is one of naming rather than of pitch.
static constexpr int kScaleDegrees = 7;
const int *scaleSteps(Mode mode);

// MIDI note number for a scale degree, where degree 0 is the tonic in `octave`
// and degrees run on past 6 into the octaves above (or below, if negative).
int degreeToMidi(const Key &key, int degree, int octave = 4);

} // namespace chalkwalk::music::Notation
