#include <unity.h>
#include "wis_calc.h"

// Mirrors the WIS_THRESHOLD_FLOOR from config.h
// Defined here explicitly so tests are self-documenting
#define FLOOR 10.0f

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// computeWisPct — normal cases
// ---------------------------------------------------------------------------

void test_wis_pct_typical(void) {
    // 14.49 / 147.16 = 9.8% → truncated to 9
    TEST_ASSERT_EQUAL(9, computeWisPct(14.49f, 147.16f, FLOOR));
}

void test_wis_pct_half_threshold(void) {
    // Exactly 50% of threshold
    TEST_ASSERT_EQUAL(50, computeWisPct(73.58f, 147.16f, FLOOR));
}

void test_wis_pct_at_threshold(void) {
    // Score equals threshold → 100%
    TEST_ASSERT_EQUAL(100, computeWisPct(147.16f, 147.16f, FLOOR));
}

void test_wis_pct_above_threshold_clamped(void) {
    // Score exceeds threshold → clamped to 100
    TEST_ASSERT_EQUAL(100, computeWisPct(200.0f, 147.16f, FLOOR));
}

void test_wis_pct_just_below_threshold(void) {
    // 99% — should not clamp to 100
    TEST_ASSERT_EQUAL(99, computeWisPct(145.69f, 147.16f, FLOOR));
}

// ---------------------------------------------------------------------------
// computeWisPct — floor and edge cases
// ---------------------------------------------------------------------------

void test_wis_pct_zero_score_clamps_to_1(void) {
    // 0 / 147.16 = 0% → clamped to 1
    TEST_ASSERT_EQUAL(1, computeWisPct(0.0f, 147.16f, FLOOR));
}

void test_wis_pct_very_small_score_clamps_to_1(void) {
    // Tiny score well below 1%
    TEST_ASSERT_EQUAL(1, computeWisPct(0.1f, 147.16f, FLOOR));
}

void test_wis_pct_threshold_below_floor_returns_1(void) {
    // Threshold of 5 is below floor of 10 → return 1 regardless of score
    TEST_ASSERT_EQUAL(1, computeWisPct(100.0f, 5.0f, FLOOR));
}

void test_wis_pct_threshold_exactly_at_floor(void) {
    // Threshold exactly equals floor — the check is strict (<), so 10.0 is NOT
    // below 10.0 and normal calculation runs: 5.0 / 10.0 * 100 = 50%
    TEST_ASSERT_EQUAL(50, computeWisPct(5.0f, 10.0f, FLOOR));
}

void test_wis_pct_threshold_just_above_floor(void) {
    // Threshold just above floor → normal calculation applies
    // 5.5 / 10.1 * 100 = 54.4% → 54
    TEST_ASSERT_EQUAL(54, computeWisPct(5.5f, 10.1f, FLOOR));
}

void test_wis_pct_zero_threshold_returns_1(void) {
    // Threshold = 0 would cause divide-by-zero — floor protection returns 1
    TEST_ASSERT_EQUAL(1, computeWisPct(50.0f, 0.0f, FLOOR));
}

// ---------------------------------------------------------------------------
// computeIsLive — live detection from mode string
// ---------------------------------------------------------------------------

void test_is_live_mode_off_is_false(void) {
    TEST_ASSERT_FALSE(computeIsLive("off"));
}

void test_is_live_mode_live_is_true(void) {
    TEST_ASSERT_TRUE(computeIsLive("live"));
}

void test_is_live_mode_on_is_true(void) {
    TEST_ASSERT_TRUE(computeIsLive("on"));
}

void test_is_live_mode_active_is_true(void) {
    TEST_ASSERT_TRUE(computeIsLive("active"));
}

void test_is_live_empty_string_is_true(void) {
    // An empty mode is unexpected but not "off" → treat as live (fail open)
    TEST_ASSERT_TRUE(computeIsLive(""));
}

void test_is_live_null_is_false(void) {
    // Null mode pointer → safe default of not-live
    TEST_ASSERT_FALSE(computeIsLive(nullptr));
}

void test_is_live_case_sensitive(void) {
    // "Off" (capitalised) is not "off" → treated as live
    TEST_ASSERT_TRUE(computeIsLive("Off"));
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_wis_pct_typical);
    RUN_TEST(test_wis_pct_half_threshold);
    RUN_TEST(test_wis_pct_at_threshold);
    RUN_TEST(test_wis_pct_above_threshold_clamped);
    RUN_TEST(test_wis_pct_just_below_threshold);
    RUN_TEST(test_wis_pct_zero_score_clamps_to_1);
    RUN_TEST(test_wis_pct_very_small_score_clamps_to_1);
    RUN_TEST(test_wis_pct_threshold_below_floor_returns_1);
    RUN_TEST(test_wis_pct_threshold_exactly_at_floor);
    RUN_TEST(test_wis_pct_threshold_just_above_floor);
    RUN_TEST(test_wis_pct_zero_threshold_returns_1);
    RUN_TEST(test_is_live_mode_off_is_false);
    RUN_TEST(test_is_live_mode_live_is_true);
    RUN_TEST(test_is_live_mode_on_is_true);
    RUN_TEST(test_is_live_mode_active_is_true);
    RUN_TEST(test_is_live_empty_string_is_true);
    RUN_TEST(test_is_live_null_is_false);
    RUN_TEST(test_is_live_case_sensitive);

    return UNITY_END();
}
