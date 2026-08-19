// SPDX-License-Identifier: MIT
// Part of chalkwalk-music. See LICENSE.
#pragma once

// Which note, given where the last one was.
//
// `NoteStrength.h` answers "how good is this note here" and stops there. It is
// a judgement about one note against the harmony, and it has no memory. Two
// generators built on it produced lines that were harmonically defensible note
// by note and did not sound like melodies, for a reason that is obvious once
// stated: a melody is mostly a fact about INTERVALS, and nothing in the model
// knew what the previous note was.
//
// ---------------------------------------------------------------------------
// THE GATE AND THE OBJECTIVE ARE DIFFERENT THINGS
//
// The tempting move is to add an interval penalty to the note's strength and
// take the minimum of the sum. Do not. Strength is a tier order -- chord root,
// chord tone, key root, scale tone, and so on down -- and it is a hard ranking
// on purpose, because a strong beat taking a clashing chromatic sounds wrong
// no matter how smoothly it was approached. Adding a leap penalty into that
// sum lets an awkward interval buy its way into a worse tier.
//
// So the two roles stay separate:
//
//   GATE       metric strength -> `rankCeiling` -> which notes are ADMISSIBLE
//   OBJECTIVE  contour distance + interval cost -> which admissible note WINS
//
// The gate is a hard constraint and the objective never crosses it. That also
// makes the interesting property testable in one line: no note above the
// ceiling is ever chosen.
//
// This replaces `snapToRank`, which searched outward from the note nearest the
// contour until it found one the beat allowed. That is a greedy approximation
// of this minimisation, and its failure mode is exactly the one that prompted
// this: when the nearest note is inadmissible, the outward walk can land a long
// way off, with no term objecting to the size of the jump it just made.
//
// ---------------------------------------------------------------------------
// DIRECTION IS A SECOND KIND OF MEMORY
//
// Knowing how far the last note was is not the same as knowing which way it
// went. Two rules exist about that and they look contradictory:
//
//   A RUN READS AS INTENTIONAL. Having moved up by step, moving up again says
//   the line meant it; turning round says it was wandering.
//
//   A LEAP WANTS FILLING IN. Having jumped a fifth, coming back down is what
//   the ear expects -- this is the counterpoint gap-fill rule, and it is why
//   an octave leap followed by a further octave leap sounds like a mistake.
//
// They are not in conflict, they apply at different SIZES, so one term
// captures both with the regime chosen by how big the previous move was. See
// `directionCost`.
//
// Direction and the phrase contour are the same kind of statement at different
// time scales -- the contour says where the line should be by the end, this
// says what it should do next -- so they conflict only at a contour's turning
// point. The caller decides how loudly to state it, and the weight that makes
// sense depends on the contour it is competing with: a rising contour already
// dictates direction and needs almost none, where a random walk has no shape
// of its own and this is the entire difference between wandering and phrasing.
//
// ---------------------------------------------------------------------------
// WHY THE COST IS NOT MONOTONE IN SIZE
//
// A tenth is not "worse than" an octave because it is bigger. Melodic leaps are
// idiomatic or they are not, and that is a fact about the INTERVAL rather than
// the distance: the fourth and the fifth are the horn call and the fanfare, the
// octave is a deliberate gesture every listener parses instantly, and the
// tritone and the major seventh are none of those things at any size. So the
// table below dips at 5, 7 and 12 and peaks either side of them.

#include <cstdlib>
#include <vector>

namespace chalkwalk::music {

// What it costs the line to move by `semitones`, in units of "semitones away
// from where the contour wanted to be" -- so a cost of 4 says this leap is
// worth taking only if it lands the line four semitones closer to its shape.
//
// Stepwise motion is free because it is the default state of a melody, not
// because it is cheap. Everything else is priced against it.
//
// THE UNISON IS NOT THE CHEAPEST MOVE, and that is the second place this table
// deliberately stops being monotone. Repeating a note is not melodic motion at
// all, and if standing still is free then every other term in the objective is
// an argument for standing still: measured with the unison at 0, turning the
// direction weight up took repeated notes from 20% of all moves to 54%, which
// is a drone rather than a melody. Pricing it level with a third is what makes
// the direction term usable.
[[nodiscard]] inline int intervalCost(int semitones) noexcept {
  const int d = std::abs(semitones);
  if (d == 0)
    return 1;   // NOT motion. See below.
  if (d <= 2)
    return 0;   // stepwise: what a melody does unless it has a reason
  if (d <= 4)
    return 1;   // thirds: barely a leap
  if (d == 5)
    return 2;   // perfect fourth -- as idiomatic a leap as the fifth
  if (d == 6)
    return 6;   // tritone: the one small interval that is not a step
  if (d == 7)
    return 2;   // perfect fifth
  if (d < 12)
    return 8;   // sixths and sevenths: the awkward band
  if (d == 12)
    return 4;   // the octave is heard as the same note, so it is cheap for its size
  return 10 + (d - 12);
}

// What it costs to move this way, having last moved that way.
//
// Non-negative throughout, deliberately: expressing gap-fill as a BONUS for
// reversing works out to the same ordering and makes the objective's scale
// depend on the history, which is harder to reason about and harder to weight.
//
//                       after a step (<=4)   after a leap (>=5)
//     continuing                 0                   3
//     reversing                  2                   0
//
// A fourth counts as a leap here. Gap-fill is usually stated for a fourth and
// wider, and the tritone -- the other interval in that band -- wants resolving
// in one direction anyway.
[[nodiscard]] inline int directionCost(int move, int lastMove) noexcept {
  if (move == 0 || lastMove == 0)
    return 0;
  const bool continuing = (move > 0) == (lastMove > 0);
  if (std::abs(lastMove) <= 4)
    return continuing ? 0 : 2;
  return continuing ? 3 : 0;
}

// What the line remembers. `lastMove` is signed and holds the last NON-ZERO
// move, so a repeated note does not erase which way the line was going.
struct MelodyState {
  int lastNote = -1;   // negative at the start of a line
  int lastMove = 0;    // 0 until the line has actually moved

  // Fold a chosen note in. Kept here rather than at each call site because
  // "only update lastMove when the note actually moved" is the kind of rule
  // that gets written correctly once and wrongly thereafter.
  void advance(int note) noexcept {
    if (lastNote >= 0 && note != lastNote)
      lastMove = note - lastNote;
    lastNote = note;
  }
};

// How the halves of the objective trade off.
//
// `contour` is the cost of one semitone away from where the phrase shape wants
// to be, and is the unit everything else is quoted in. Raising `interval`
// buys smoother lines at the price of a contour that is followed more loosely;
// at 0 this degrades exactly to "nearest admissible note to the contour", which
// is what the callers did before this existed.
struct MelodyWeights {
  int contour = 1;
  int interval = 1;
  int direction = 0;   // off by default: it is the caller who knows the contour
};

// DIRECTION MUST NOT BE ABLE TO OUTBID INTERVAL. Keep it at or below half the
// interval weight.
//
// Gap-fill makes reversing after a leap free, so a large downward reversal is
// competing on contour distance alone -- and when the contour happens to want
// to go that way too, the two agree and the interval cost loses. Measured on a
// real generator at interval weight 2: at direction 1 the wide leaps stay at 6
// per 1939 moves, at 2 they jump to 41, and at 3 to 85 with the largest
// interval growing from an octave to a minor fourteenth. That is the exact
// fault the interval term exists to prevent, reintroduced by its own
// tie-breaker.
//
// A caller exposing this as a control should therefore DERIVE the direction
// weight from the interval weight rather than offering it separately, or the
// two dials have a region where they cancel.
[[nodiscard]] inline constexpr int safeDirectionWeight(int intervalWeight) noexcept {
  return intervalWeight > 0 ? intervalWeight / 2 : 0;
}

// The admissible candidate that best serves the contour and the previous note.
//
// `candidates` are MIDI notes in ascending order and `ranks` their strengths,
// one per candidate; `ceiling` is `rankCeiling(metricStrength, hasChart)`.
// `aim` is where the phrase shape wants the line, and `state` is what the line
// remembers. At the start of a line -- `lastNote` negative -- the interval and
// direction terms contribute nothing and this is pure contour following.
//
// Ties resolve to the lower index, and since callers pass sorted candidates
// that means the lower pitch. Defined rather than incidental, because a
// generator whose ties fall differently between two runs is not reproducible
// from its seed.
//
// If nothing at all satisfies the ceiling the same objective is minimised over
// every candidate. A caller whose pool cannot satisfy its own gate has a bug,
// but silently returning an arbitrary index would hide it in something that
// still sounds nearly right.
[[nodiscard]] inline std::size_t chooseNote(const std::vector<int> &candidates,
                                            const std::vector<int> &ranks,
                                            int ceiling, int aim,
                                            const MelodyState &state,
                                            const MelodyWeights &w = {}) noexcept {
  const std::size_t n = candidates.size();
  if (n == 0)
    return 0;

  auto scoreOf = [&](std::size_t i) {
    int score = w.contour * std::abs(candidates[i] - aim);
    if (state.lastNote >= 0) {
      const int move = candidates[i] - state.lastNote;
      score += w.interval * intervalCost(move);
      score += w.direction * directionCost(move, state.lastMove);
    }
    return score;
  };

  std::size_t best = n;
  int bestScore = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (i < ranks.size() && ranks[i] > ceiling)
      continue;
    const int score = scoreOf(i);
    if (best == n || score < bestScore) {
      best = i;
      bestScore = score;
    }
  }
  if (best != n)
    return best;

  best = 0;
  bestScore = scoreOf(0);
  for (std::size_t i = 1; i < n; ++i) {
    const int score = scoreOf(i);
    if (score < bestScore) {
      best = i;
      bestScore = score;
    }
  }
  return best;
}

}  // namespace chalkwalk::music
