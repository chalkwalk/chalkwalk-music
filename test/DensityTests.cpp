// SPDX-License-Identifier: MIT
//
// Ported from Lockstep\'s suite with the assertions unchanged.

#include "LegacyCheck.h"
#include <chalkwalk/music/Density.h>
#include <cmath>
#include <cstdint>

namespace {

using namespace chalkwalk::music;
    using namespace Density;

    // -----------------------------------------------------------------------
    // metricDrop ordering (4/4, num=4 denom=4)

    TEST_CASE("metric drop downbeat", "[density]")
    {
        // 4/4 bar at 480 PPQ/beat = 1920 PPQ/bar.
        constexpr double barPpq = 1920.0;
        float db = metricDrop(0.0, barPpq, 4, 4);
        CHECK_MSG(db == 0.0f, "metricDrop: downbeat = 0 (survives longest)");
    }

    TEST_CASE("metric drop half bar", "[density]")
    {
        constexpr double barPpq = 1920.0;
        constexpr double sixteenth = barPpq / 16.0;
        float db    = metricDrop(0.0,           barPpq, 4, 4); // beat 1
        float beat3 = metricDrop(8.0 * sixteenth, barPpq, 4, 4); // beat 3
        float beat2 = metricDrop(4.0 * sixteenth, barPpq, 4, 4); // beat 2
        // In the L-J hierarchy beat 3 is the bar's 2nd-level downbeat:
        // stronger than beats 2,4 but weaker than the bar downbeat.
        CHECK_MSG(db < beat3,   "metricDrop: beat 3 is weaker than the bar downbeat");
        CHECK_MSG(beat3 < beat2, "metricDrop: beat 3 is stronger than beat 2");
    }

    TEST_CASE("metric drop ordering", "[density]")
    {
        // Verify: downbeat ≤ beat3 ≤ beat2 ≤ "& " ≤ sixteenth-offbeat.
        constexpr double barPpq = 1920.0;
        constexpr double sixteenth = barPpq / 16.0;

        float db      = metricDrop(0.0,            barPpq, 4, 4); // k=0
        float beat3   = metricDrop(8.0 * sixteenth, barPpq, 4, 4); // k=8
        float beat2   = metricDrop(4.0 * sixteenth, barPpq, 4, 4); // k=4
        float eighth  = metricDrop(2.0 * sixteenth, barPpq, 4, 4); // k=2
        float sixtn   = metricDrop(1.0 * sixteenth, barPpq, 4, 4); // k=1

        CHECK_MSG(db < beat3,   "ordering: downbeat < beat 3");
        CHECK_MSG(beat3 < beat2, "ordering: beat 3 < beat 2");
        CHECK_MSG(beat2 < eighth, "ordering: beat 2 < eighth offbeat");
        CHECK_MSG(eighth < sixtn, "ordering: eighth < sixteenth offbeat");
        CHECK_MSG(sixtn >= 0.99f, "metricDrop: finest 16th offbeat = 1 (dies first)");

        float beat4 = metricDrop(12.0 * sixteenth, barPpq, 4, 4); // k=12, same as beat 2
        CHECK_MSG(std::abs(beat2 - beat4) < 0.001f, "metricDrop: beats 2 and 4 equal propensity");
    }

    TEST_CASE("metric drop edge cases", "[density]")
    {
        // barPpq = 0 should not crash (returns 0).
        float r = metricDrop(100.0, 0.0, 4, 4);
        CHECK_MSG(r == 0.0f, "metricDrop: barPpq=0 returns 0 safely");

        // ppqInBar > barPpq: fmod keeps it in range.
        float wrapped = metricDrop(1921.0, 1920.0, 4, 4);
        CHECK_MSG(wrapped >= 0.0f && wrapped <= 1.0f, "metricDrop: ppqInBar > barPpq wraps safely");
    }

    // -----------------------------------------------------------------------
    // Per-meter ordering tests (the point of the MetricGrid refactor)

    TEST_CASE("metric drop seven eight", "[density]")
    {
        // 7/8: 3+2+2 → group heads at pulses 0, 3, 5.
        // Pulse weights: [3,1,1,2,1,2,1] → group heads have lower metricDrop.
        // barPpq for 7/8 at 480 PPQ/quarter: 7*(4.0/8)*480 = 1680 PPQ.
        constexpr double barPpq = 1680.0;
        constexpr double pulsePpq = barPpq / 7.0; // one eighth note

        float p0 = metricDrop(0.0 * pulsePpq, barPpq, 7, 8); // pulse 0 (bar head)
        float p1 = metricDrop(1.0 * pulsePpq, barPpq, 7, 8); // pulse 1 (inner)
        float p3 = metricDrop(3.0 * pulsePpq, barPpq, 7, 8); // pulse 3 (group head)
        float p5 = metricDrop(5.0 * pulsePpq, barPpq, 7, 8); // pulse 5 (group head)

        CHECK_MSG(p0 == 0.0f, "7/8: bar downbeat = 0 drop");
        CHECK_MSG(p1 > p0,    "7/8: inner pulses weaker than downbeat");
        CHECK_MSG(p3 < p1,    "7/8: group head (pulse 3) stronger than inner (pulse 1)");
        CHECK_MSG(p5 < p1,    "7/8: group head (pulse 5) stronger than inner (pulse 1)");
        // Pulses 3 and 5 are both group heads of 2-element groups → same weight.
        CHECK_MSG(std::abs(p3 - p5) < 0.001f, "7/8: group heads 3 and 5 equal strength");
        // Pulses 1 and 2 (inner, w=1) are both weakest → equal.
        float p4 = metricDrop(4.0 * pulsePpq, barPpq, 7, 8);
        CHECK_MSG(std::abs(p1 - p4) < 0.001f, "7/8: inner pulses 1 and 4 equal strength");
    }

    TEST_CASE("metric drop six eight", "[density]")
    {
        // 6/8: two dotted-quarter groups (3+3); group heads at pulses 0 and 3.
        // Pulse weights: [3,1,1,2,1,1].
        constexpr double barPpq = 1920.0 * 0.75; // 6/8 at 480 PPQ/quarter = 1440
        constexpr double pulsePpq = barPpq / 6.0;

        float p0 = metricDrop(0.0 * pulsePpq, barPpq, 6, 8); // bar head
        float p1 = metricDrop(1.0 * pulsePpq, barPpq, 6, 8); // inner
        float p3 = metricDrop(3.0 * pulsePpq, barPpq, 6, 8); // second group head

        CHECK_MSG(p0 == 0.0f, "6/8: bar downbeat = 0 drop");
        CHECK_MSG(p3 > p0,    "6/8: second group head weaker than bar downbeat");
        CHECK_MSG(p3 < p1,    "6/8: second group head (pulse 3) stronger than inner (pulse 1)");
    }

    TEST_CASE("metric drop three four", "[density]")
    {
        // 3/4: pulse weights [2,1,1]. Pulse 0 strongest, pulses 1 and 2 equal.
        constexpr double barPpq = 1920.0 * 0.75; // 3 quarter-notes at 480 PPQ
        constexpr double pulsePpq = barPpq / 3.0;

        float p0 = metricDrop(0.0 * pulsePpq, barPpq, 3, 4);
        float p1 = metricDrop(1.0 * pulsePpq, barPpq, 3, 4);
        float p2 = metricDrop(2.0 * pulsePpq, barPpq, 3, 4);

        CHECK_MSG(p0 == 0.0f, "3/4: bar downbeat = 0 drop");
        CHECK_MSG(p1 > p0, "3/4: beat 2 weaker than downbeat");
        CHECK_MSG(std::abs(p1 - p2) < 0.001f, "3/4: beats 2 and 3 equal");

        // 3/4 and 6/8 differ: in 3/4 there is no secondary group head.
        constexpr double barPpq6 = 1920.0 * 0.75;
        constexpr double pulsePpq6 = barPpq6 / 6.0;
        float sixEightP3 = metricDrop(3.0 * pulsePpq6, barPpq6, 6, 8);
        float threeFourP1 = metricDrop(1.0 * pulsePpq, barPpq, 3, 4);
        // 6/8's second group head (pulse 3) is stronger than 3/4's beat 2.
        CHECK_MSG(sixEightP3 < threeFourP1, "6/8 group head stronger than 3/4 beat 2 (meters differ)");
    }

    // -----------------------------------------------------------------------
    // densityScrubHash decorrelation

    TEST_CASE("scrub hash decorrelation", "[density]")
    {
        // For the same (trackIdx, stepPos) the Scrub hash must produce a different
        // distribution than TrigEvaluator::deterministicPercent.
        auto tevHash = [](std::size_t t, std::int64_t s) -> int {
            auto h = (static_cast<uint32_t>(t) * 2654435761u)
                   ^ static_cast<uint32_t>(static_cast<uint64_t>(s) * 2246822519ull);
            h ^= h >> 16u;
            h *= 0x45d9f3bu;
            h ^= h >> 16u;
            return static_cast<int>(h % 100u);
        };

        int sameCount = 0;
        for (int i = 0; i < 1000; ++i)
        {
            const auto t = static_cast<std::size_t>(i);
            const auto s = static_cast<std::int64_t>(i);
            int density_pct = static_cast<int>(densityScrubHash(t, s, 50) % 100u);
            int tev_pct = tevHash(t, s);
            if (density_pct == tev_pct) { ++sameCount; }
        }
        // Over 1000 comparisons in [0,99] ≈10% coincidental matches expected;
        // >500 would indicate strong correlation.
        CHECK_MSG(sameCount < 200, "scrubHash: low accidental correlation with deterministicPercent");
    }

    TEST_CASE("scrub hash reshuffles", "[density]")
    {
        // Changing quantizedKnobLevel should change most step selections.
        int sameCount = 0;
        for (int step = 0; step < 200; ++step)
        {
            auto h50 = densityScrubHash(0, static_cast<std::int64_t>(step), 50);
            auto h51 = densityScrubHash(0, static_cast<std::int64_t>(step), 51);
            if ((h50 % 10000u) == (h51 % 10000u)) { ++sameCount; }
        }
        CHECK_MSG(sameCount < 20, "scrubHash: knob-level change reshuffles most steps");
    }

    // -----------------------------------------------------------------------
    // densitySurvives — statistical properties

    TEST_CASE("surviving count uniform", "[density]")
    {
        constexpr int kSteps = 10000;
        constexpr float kEffective = 0.5f;
        constexpr double kBarPpq = 1920.0;
        const int qLevel = static_cast<int>(std::round(kEffective * 100.0f));

        int survivedScrub = 0;
        int survivedReroll = 0;
        for (int i = 0; i < kSteps; ++i)
        {
            const double ppqInBar = kBarPpq * (static_cast<double>(i % 16) / 16.0);
            const float rerollR = static_cast<float>(i % 997) / 997.0f;
            const auto ti = static_cast<std::size_t>(i);
            const auto si = static_cast<std::int64_t>(i);

            if (densitySurvives(kEffective, 0.0f, ppqInBar, kBarPpq, 4, 4,
                                Musicality::Uniform, DensitySelection::Scrub,
                                ti, si, qLevel, rerollR))
            {
                ++survivedScrub;
            }
            if (densitySurvives(kEffective, 0.0f, ppqInBar, kBarPpq, 4, 4,
                                Musicality::Uniform, DensitySelection::Reroll,
                                ti, si, qLevel, rerollR))
            {
                ++survivedReroll;
            }
        }
        CHECK_MSG(survivedScrub > 4000 && survivedScrub < 6000,
              "densitySurvives Uniform/Scrub: surviving count ≈ 50% (±10%)");
        CHECK_MSG(survivedReroll > 4000 && survivedReroll < 6000,
              "densitySurvives Uniform/Reroll: surviving count ≈ 50% (±10%)");
    }

    TEST_CASE("surviving count metric", "[density]")
    {
        // Metric mode changes *which* steps survive based on metric importance.
        // With the importance→probability model, average p ≈ effective across the grid.
        constexpr int kSteps = 10000;
        constexpr float kEffective = 0.5f;
        constexpr double kBarPpq = 1920.0;
        const int qLevel = static_cast<int>(std::round(kEffective * 100.0f));

        int survived = 0;
        for (int i = 0; i < kSteps; ++i)
        {
            const double ppqInBar = kBarPpq * (static_cast<double>(i % 16) / 16.0);
            const float rerollR = static_cast<float>(i % 997) / 997.0f;
            if (densitySurvives(kEffective, 0.0f, ppqInBar, kBarPpq, 4, 4,
                                Musicality::Metric, DensitySelection::Scrub,
                                static_cast<std::size_t>(i), static_cast<std::int64_t>(i),
                                qLevel, rerollR))
            {
                ++survived;
            }
        }
        // In Metric mode the weight distribution is non-uniform: most bar positions
        // are weak sub-pulse offbeats (w≈0) so the actual survival rate in a 16-step
        // 4/4 scan is ~26% (~0.9 * 0.5 on weak positions → p≈0.05).  Verify it is
        // non-trivial (>10%) and well below 100% (the distribution is active).
        CHECK_MSG(survived > 1000 && survived < 7000,
              "densitySurvives Metric: survival count non-trivial (metric mode active)");
    }

    // Regression guard: Scrub and Reroll must produce different surviving sets
    // in Metric mode. Pre-fix, the selection value had zero weight at m=1, so
    // both modes produced identical results (the zero-weight bug).
    TEST_CASE("metric scrub vs reroll differ", "[density]")
    {
        constexpr float kEffective = 0.5f;
        constexpr double kBarPpq = 1920.0;
        constexpr double kSixteenth = kBarPpq / 16.0;
        const int qLevel = static_cast<int>(std::round(kEffective * 100.0f));

        int sameCount = 0;
        constexpr int kTrials = 500;
        for (int i = 0; i < kTrials; ++i)
        {
            const double ppqInBar = kSixteenth * static_cast<double>(i % 16);
            const float rerollR = static_cast<float>(i % 997) / 997.0f;
            const auto ti = static_cast<std::size_t>(i);
            const auto si = static_cast<std::int64_t>(i);

            const bool scrub  = densitySurvives(kEffective, 0.0f, ppqInBar, kBarPpq, 4, 4,
                                                Musicality::Metric, DensitySelection::Scrub,
                                                ti, si, qLevel, rerollR);
            const bool reroll = densitySurvives(kEffective, 0.0f, ppqInBar, kBarPpq, 4, 4,
                                                Musicality::Metric, DensitySelection::Reroll,
                                                ti, si, qLevel, rerollR);
            if (scrub == reroll) { ++sameCount; }
        }
        CHECK_MSG(sameCount < kTrials,
              "densitySurvives Metric: Scrub and Reroll produce different selections");
    }

    // Gradual thinning: as density decreases from 1, offbeats drop before downbeats.
    TEST_CASE("metric gradual thinning", "[density]")
    {
        constexpr double kBarPpq = 1920.0;
        constexpr double kSixteenth = kBarPpq / 16.0;
        constexpr int kTrials = 1000;

        auto countSurvived = [&](float effective, double ppqInBar) -> int {
            const int qLevel = static_cast<int>(std::round(effective * 100.0f));
            int count = 0;
            for (int i = 0; i < kTrials; ++i)
            {
                const auto ti = static_cast<std::size_t>(i * 7 + 3);
                const auto si = static_cast<std::int64_t>(i);
                const float rerollR = static_cast<float>(i % 997) / 997.0f;
                if (densitySurvives(effective, 0.0f, ppqInBar, kBarPpq, 4, 4,
                                    Musicality::Metric, DensitySelection::Scrub,
                                    ti, si, qLevel, rerollR))
                {
                    ++count;
                }
            }
            return count;
        };

        // At low density (0.15), downbeats survive far more than sixteenth offbeats.
        const int downbeat  = countSurvived(0.15f, 0.0);               // w=1
        const int sixteenth = countSurvived(0.15f, 1.0 * kSixteenth);  // w=0

        CHECK_MSG(downbeat > sixteenth * 3,
              "gradual thinning: downbeats survive significantly more than finest offbeats");
        CHECK_MSG(downbeat > 0,   "gradual thinning: downbeats still survive at density=0.15");
        CHECK_MSG(sixteenth < kTrials, "gradual thinning: sixteenth offbeats thinned at density=0.15");
    }

    TEST_CASE("floor and ceiling", "[density]")
    {
        // Ceiling: effective=1.0 → all steps survive.
        int full = 0;
        for (int i = 0; i < 1000; ++i)
        {
            const float rerollR = static_cast<float>(i % 997) / 997.0f;
            if (densitySurvives(1.0f, 0.0f, 0.0, 1920.0, 4, 4,
                                Musicality::Uniform, DensitySelection::Reroll,
                                0, static_cast<std::int64_t>(i), 100, rerollR))
            {
                ++full;
            }
        }
        CHECK_MSG(full == 1000, "densitySurvives: effective=1.0 → all steps survive");

        // Floor: at effective=0.01 very few steps survive (~1%); verify < 10%.
        int floored = 0;
        for (int i = 0; i < 1000; ++i)
        {
            const float rerollR = static_cast<float>(i % 997) / 997.0f;
            if (densitySurvives(0.01f, -1.0f, 0.0, 1920.0, 4, 4,
                                Musicality::Uniform, DensitySelection::Scrub,
                                0, static_cast<std::int64_t>(i), 1, rerollR))
            {
                ++floored;
            }
        }
        CHECK_MSG(floored < 100, "densitySurvives: effective=1% → very few steps survive");
    }

    TEST_CASE("metric mode ordering", "[density]")
    {
        // Metric mode: downbeats survive more than finest offbeats.
        constexpr float kEffective = 0.5f;
        constexpr double kBarPpq = 1920.0;
        constexpr double kSixteenth = kBarPpq / 16.0;
        const int qLevel = static_cast<int>(std::round(kEffective * 100.0f));

        int survivedDownbeat = 0;
        int survivedOffbeat = 0;
        constexpr int kTrials = 1000;
        for (int i = 0; i < kTrials; ++i)
        {
            const float rerollR = static_cast<float>(i % 997) / 997.0f;
            const auto si = static_cast<std::int64_t>(i);
            const std::size_t ti = (static_cast<std::size_t>(i) * 7u) + 3u;

            if (densitySurvives(kEffective, 0.0f, 0.0, kBarPpq, 4, 4,
                                Musicality::Metric, DensitySelection::Scrub,
                                ti, si, qLevel, rerollR))
            {
                ++survivedDownbeat;
            }
            if (densitySurvives(kEffective, 0.0f, 1.0 * kSixteenth, kBarPpq, 4, 4,
                                Musicality::Metric, DensitySelection::Scrub,
                                ti, si, qLevel, rerollR))
            {
                ++survivedOffbeat;
            }
        }
        CHECK_MSG(survivedDownbeat > survivedOffbeat,
              "densitySurvives Metric: downbeats survive more than finest offbeats");
    }

    TEST_CASE("master offset", "[density]")
    {
        // Positive master offset raises effective density → more survivors.
        constexpr double kBarPpq = 1920.0;
        int survivedLow = 0;
        int survivedHigh = 0;
        for (int i = 0; i < 1000; ++i)
        {
            const float rerollR = static_cast<float>(i % 997) / 997.0f;
            const double ppq = kBarPpq * (static_cast<double>(i % 16) / 16.0);
            const auto si = static_cast<std::int64_t>(i);

            if (densitySurvives(0.3f, 0.0f, ppq, kBarPpq, 4, 4,
                                Musicality::Uniform, DensitySelection::Reroll,
                                0u, si, 30, rerollR))
            {
                ++survivedLow;
            }
            if (densitySurvives(0.3f, 0.4f, ppq, kBarPpq, 4, 4,
                                Musicality::Uniform, DensitySelection::Reroll,
                                0u, si, 70, rerollR))
            {
                ++survivedHigh;
            }
        }
        CHECK_MSG(survivedHigh > survivedLow,
              "densitySurvives: positive master offset raises survival rate");
    }

    // -----------------------------------------------------------------------
    // Reroll cadence helpers

    TEST_CASE("reroll per step evolves", "[density]")
    {
        int sameCount = 0;
        constexpr int kN = 500;
        for (int i = 0; i < kN - 1; ++i)
        {
            float r0 = rerollPerStep(0u, static_cast<std::int64_t>(i));
            float r1 = rerollPerStep(0u, static_cast<std::int64_t>(i + 1));
            if (r0 == r1) { ++sameCount; }
        }
        CHECK_MSG(sameCount < 20, "rerollPerStep: consecutive steps produce different values");
    }

    TEST_CASE("reroll per bar cadence", "[density]")
    {
        const float r0 = rerollPerBar(3u, 7LL, 2LL);
        const float r1 = rerollPerBar(3u, 7LL, 2LL);
        CHECK_MSG(r0 == r1, "rerollPerBar: same inputs → same output");

        const float rBar0 = rerollPerBar(0u, 0LL, 0LL);
        const float rBar1 = rerollPerBar(0u, 1LL, 0LL);
        CHECK_MSG(rBar0 != rBar1, "rerollPerBar: bar change at same stepInBar gives new value");

        const float rStep0 = rerollPerBar(0u, 5LL, 0LL);
        const float rStep3 = rerollPerBar(0u, 5LL, 3LL);
        CHECK_MSG(rStep0 != rStep3, "rerollPerBar: different stepInBar within same bar differ");

        int sameBar = 0;
        for (std::int64_t bar = 0; bar < 99; ++bar)
        {
            if (rerollPerBar(1u, bar, 0LL) == rerollPerBar(1u, bar + 1, 0LL))
                ++sameBar;
        }
        CHECK_MSG(sameBar < 10, "rerollPerBar: value varies across bars");
    }

}  // namespace
