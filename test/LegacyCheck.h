// SPDX-License-Identifier: MIT
#pragma once

#include <catch2/catch_test_macros.hpp>

// The suites in this library were ported from Lockstep, whose test harness took
// a message as CHECK's second argument. The assertions came across unchanged --
// they are the acceptance test for each extraction, and rewriting the tests
// while moving the code they cover would defeat the point of having them.
//
// The condition is parenthesised because Catch2 declines to decompose `&&` or a
// chained comparison and several of these use both. That costs the decomposed
// failure output, which the message more than replaces.
#define CHECK_MSG(cond, msg)                                                   \
    do {                                                                       \
        INFO(msg);                                                             \
        CHECK((cond));                                                         \
    } while (false)
