// SPDX-License-Identifier: MIT
#pragma once

#include "LegacyCheck.h"

// The Harmony and Notation suites came from Antiphon, whose harness is
// `juce::UnitTest`: `expect(cond)`, `expect(cond, message)` and the same pair
// for `expectEquals`.
//
// The assertions came across unchanged, for the reason LegacyCheck.h gives --
// they are the acceptance test for the extraction, and rewriting the tests
// while moving the code they cover would defeat the point of having them. So
// the two spellings are mapped rather than edited, and the diff of the move is
// the harness and nothing else.
//
// The arity-overloaded macro is what lets `expect(x)` and `expect(x, why)`
// both survive; both spellings appear hundreds of times.

#define CW_CHECK_PICK(_1, _2, NAME, ...) NAME
#define CW_CHECK_1(cond) CHECK((cond))
#define CW_CHECK_2(cond, msg) CHECK_MSG((cond), msg)
#define EXPECT(...) CW_CHECK_PICK(__VA_ARGS__, CW_CHECK_2, CW_CHECK_1)(__VA_ARGS__)

#define CW_EQ_PICK(_1, _2, _3, NAME, ...) NAME
#define CW_EQ_2(a, b) CHECK((a) == (b))
#define CW_EQ_3(a, b, msg) CHECK_MSG((a) == (b), msg)
#define EXPECT_EQ(...) CW_EQ_PICK(__VA_ARGS__, CW_EQ_3, CW_EQ_2)(__VA_ARGS__)
