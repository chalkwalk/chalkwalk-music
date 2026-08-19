# chalkwalk-music — roadmap

What this library is missing, and what is deliberately not here.

---

## Dissonance is register-dependent, and the model is register-blind

**The largest known gap.** `NoteStrength.h` works on **pitch classes**, so
`B4`/`C5` and `B6`/`C7` are literally the same input to it. They do not sound
the same. A semitone that is unusable in a close mid-register voicing is
perfectly playable two octaves up, and the model cannot express that.

### The mechanism

Two tones are heard as rough when they fall inside one **critical band** of the
ear's frequency resolution (Plomp & Levelt, 1965). The critical band is roughly
constant in Hz at low frequencies -- about 100 Hz below 500 Hz -- and widens
much more slowly than pitch does above that. Pitch is logarithmic and the
critical band is not, so **the same musical interval spans a different number of
critical bands depending on where you play it.**

Computed with Zwicker's approximation, as bands spanned:

| | C2 | C3 | C4 | C5 | C6 | C7 |
|---|---|---|---|---|---|---|
| Semitone | 0.04 | 0.08 | 0.15 | 0.26 | 0.37 | 0.39 |
| Major third | 0.17 | 0.34 | 0.65 | 1.14 | 1.62 | 1.72 |
| Fifth | 0.32 | 0.64 | 1.24 | 2.19 | 3.11 | 3.30 |

Roughness peaks near **0.25** of a critical band and falls away to nothing by
about **1.0**.

### Two different stories, and they are not the same story

Reading that table carefully corrects a plausible-sounding but wrong summary
("intervals get muddier the lower you play them"):

- **Thirds and wider: monotone.** A fifth spans 0.32 bands at C2 and 3.3 at C7.
  It really does clean up steadily as it rises, crossing out of the roughness
  zone somewhere around C4. This is the phenomenon the orchestration tradition
  encodes as **low interval limits** -- the rule of thumb that thirds are
  unsafe below about middle C, that the bass wants fourths, fifths and octaves,
  and that close voicings belong up top.

- **The semitone: NOT monotone.** It spans 0.04 bands at C2 and only 0.39 at
  C7, so it never fully leaves the roughness zone anywhere in the musical
  range. What changes is where it sits on the curve: it is closest to the
  0.25 roughness peak around **C4–C5**, and further from it both above and
  below. That is exactly the reported observation -- `B4`/`C5` clashes,
  `B6`/`C7` is usable -- and it is a different mechanism from the low interval
  limits, not the same one extended.

  Far below, a semitone is not *rough* so much as *beating*: at C2 the two
  fundamentals are 4 Hz apart, which is a slow wobble rather than a buzz. It
  still sounds wrong, for a different reason.

### What a real model would need

- [ ] **Partials, not just fundamentals.** The reason low close voicings are
      bad on real instruments is that the *harmonics* interact, not only the
      roots. Two notes whose fundamentals beat slowly can still have their
      third and fourth partials landing inside a critical band. Any model built
      on fundamentals alone will get the bass wrong.
- [ ] **Take the curve from a source, not from arithmetic in a roadmap.** The
      numbers above establish the direction and the shape; they are not a
      shippable dissonance function. Plomp & Levelt's curve, Sethares'
      formulation, or a published low-interval-limit table are all defensible
      starting points and they disagree in the details.
- [ ] **An interface that takes MIDI notes, not pitch classes.** This is the
      breaking change. `noteStrength(key, pitchClass, chord)` becomes something
      that knows the register of both the candidate and the sounding tones.
      The pitch-class form stays for callers that genuinely have no register
      (a quantiser does not).

### What it unlocks, and why it is worth doing

The prize is in the generators. Today a candidate that clashes is simply
demoted or rejected. With register in the model, the generator gets a third
option that a musician would take without thinking:

> The `C` clashes with the `B` the chord is sounding — so play it an octave up,
> where it does not.

That turns the clash rule from a veto into a **voicing decision**, which is what
it is. It matters most where a chord sits below a melody -- the chord occupies
the muddy register and the melody the clean one, so the same pitch class is a
mistake in one octave and fine in another.

- [ ] Octave displacement as a first-class result of ranking, not a caller's
      afterthought.
- [ ] Voicing helpers: given a chord and a register, spread it so that no pair
      sits inside a critical band.

---

## Smaller, and not yet scheduled

- [ ] **Melody generation itself.** Deliberately not here. The projects that
      use this library generate melody differently -- different output shapes,
      rhythm sources and registers -- and those differences are real rather
      than accidental. The library shares the *ranking* and the metric gate;
      forcing one generator on both callers would buy a lowest common
      denominator. Revisit only if a third consumer wants the same shape.
- [ ] **Chord vocabulary.** `SoundingChord` carries a root and a pitch-class
      mask, which is what ranking needs. Naming, quality and roman numerals
      live with whoever owns the chart, because they are entangled with
      diatonic spelling. If a second project grows a chord model, compare the
      two before moving anything.
- [ ] **Scales beyond twelve.** *This is the one blocking a planned consumer.*
      Arps's `ScaleLibrary` cannot move here until it exists: its `Scale` is
      `{name, tuningName, vector<bool> stepMask}`, a variable-size mask bound to
      a tuning, so a 41-tone scale is an ordinary case. `KeySig` is a twelve-bit
      mask with a circle-of-fifths window, and that window is not decoration --
      it is where brightness, the mode names and the fifths-distance ranking all
      come from, so `NoteStrength` and `Melody` are built on it. The two are
      different objects and one does not widen into the other. Whoever takes
      this on should decide first whether the answer is a second type or a
      generalised one, because "add a `stepsPerOctave` field" is the shape that
      looks obvious and quietly breaks every ranking in the library.
      `Scale.h` is twelve-tone; `Tuning.h` handles
      arbitrary equal and unequal divisions. They do not yet meet: there is no
      way to express "the third degree of a 19-tone scale" through the scale
      model. Worth doing when something needs it.
- [ ] **Temperament error as a measure.** How far an N-tone equal division
      sits from the low harmonics, which is why 41 and 53 are more in tune than
      12. A curiosity rather than a need, but it is one function and it belongs
      next to `Tuning.h`.
