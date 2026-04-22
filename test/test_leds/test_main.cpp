#include <unity.h>
#include "led_calc.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// computeBarLeds — wis_pct → number of LEDs lit (0–10)
// ---------------------------------------------------------------------------

void test_bar_leds_at_1pct(void) {
    // round(0.1) = 0 — below the first rounding threshold
    TEST_ASSERT_EQUAL(0, computeBarLeds(1));
}

void test_bar_leds_at_5pct(void) {
    // round(0.5) = 1
    TEST_ASSERT_EQUAL(1, computeBarLeds(5));
}

void test_bar_leds_at_10pct(void) {
    // round(1.0) = 1
    TEST_ASSERT_EQUAL(1, computeBarLeds(10));
}

void test_bar_leds_at_14pct(void) {
    // round(1.4) = 1
    TEST_ASSERT_EQUAL(1, computeBarLeds(14));
}

void test_bar_leds_at_15pct(void) {
    // round(1.5) = 2
    TEST_ASSERT_EQUAL(2, computeBarLeds(15));
}

void test_bar_leds_at_50pct(void) {
    // round(5.0) = 5 — exactly half
    TEST_ASSERT_EQUAL(5, computeBarLeds(50));
}

void test_bar_leds_at_90pct(void) {
    // round(9.0) = 9
    TEST_ASSERT_EQUAL(9, computeBarLeds(90));
}

void test_bar_leds_at_95pct(void) {
    // round(9.5) = 10
    TEST_ASSERT_EQUAL(10, computeBarLeds(95));
}

void test_bar_leds_at_100pct(void) {
    // round(10.0) = 10 — all LEDs lit
    TEST_ASSERT_EQUAL(10, computeBarLeds(100));
}

void test_bar_leds_clamped_above_max(void) {
    // Over 100 should still return max LED count
    TEST_ASSERT_EQUAL(10, computeBarLeds(150));
}

void test_bar_leds_clamped_below_zero(void) {
    // Negative input should return 0
    TEST_ASSERT_EQUAL(0, computeBarLeds(-10));
}

// ---------------------------------------------------------------------------
// computeColorTier — wis_pct → color band
// ---------------------------------------------------------------------------

void test_color_tier_min_is_blue(void) {
    TEST_ASSERT_EQUAL(TIER_BLUE, computeColorTier(1));
}

void test_color_tier_low_boundary_is_blue(void) {
    // COLOR_TIER_LOW = 40 — boundary is inclusive Blue
    TEST_ASSERT_EQUAL(TIER_BLUE, computeColorTier(40));
}

void test_color_tier_above_low_is_teal(void) {
    TEST_ASSERT_EQUAL(TIER_TEAL, computeColorTier(41));
}

void test_color_tier_mid_boundary_is_teal(void) {
    // COLOR_TIER_MID = 70 — boundary is inclusive Teal
    TEST_ASSERT_EQUAL(TIER_TEAL, computeColorTier(70));
}

void test_color_tier_above_mid_is_magenta(void) {
    TEST_ASSERT_EQUAL(TIER_MAGENTA, computeColorTier(71));
}

void test_color_tier_max_is_magenta(void) {
    TEST_ASSERT_EQUAL(TIER_MAGENTA, computeColorTier(100));
}

// ---------------------------------------------------------------------------
// computeStrobe — strobe activates at exactly 100%
// ---------------------------------------------------------------------------

void test_strobe_off_below_100(void) {
    TEST_ASSERT_FALSE(computeStrobe(99));
}

void test_strobe_off_at_50(void) {
    TEST_ASSERT_FALSE(computeStrobe(50));
}

void test_strobe_on_at_100(void) {
    TEST_ASSERT_TRUE(computeStrobe(100));
}

void test_strobe_on_above_100(void) {
    // Should still strobe if somehow over 100
    TEST_ASSERT_TRUE(computeStrobe(110));
}

// ---------------------------------------------------------------------------
// Combined — verify bar + color consistency at key thresholds
// ---------------------------------------------------------------------------

void test_9pct_is_1_led_blue(void) {
    // Today's typical WIS (14.49 / 147.16 ≈ 9%)
    int leds = computeBarLeds(9);
    ColorTier color = computeColorTier(9);
    TEST_ASSERT_EQUAL(1, leds);
    TEST_ASSERT_EQUAL(TIER_BLUE, color);
}

void test_50pct_is_5_leds_teal(void) {
    int leds = computeBarLeds(50);
    ColorTier color = computeColorTier(50);
    TEST_ASSERT_EQUAL(5, leds);
    TEST_ASSERT_EQUAL(TIER_TEAL, color);
}

void test_100pct_is_10_leds_magenta_with_strobe(void) {
    int leds = computeBarLeds(100);
    ColorTier color = computeColorTier(100);
    bool strobe = computeStrobe(100);
    TEST_ASSERT_EQUAL(10, leds);
    TEST_ASSERT_EQUAL(TIER_MAGENTA, color);
    TEST_ASSERT_TRUE(strobe);
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_bar_leds_at_1pct);
    RUN_TEST(test_bar_leds_at_5pct);
    RUN_TEST(test_bar_leds_at_10pct);
    RUN_TEST(test_bar_leds_at_14pct);
    RUN_TEST(test_bar_leds_at_15pct);
    RUN_TEST(test_bar_leds_at_50pct);
    RUN_TEST(test_bar_leds_at_90pct);
    RUN_TEST(test_bar_leds_at_95pct);
    RUN_TEST(test_bar_leds_at_100pct);
    RUN_TEST(test_bar_leds_clamped_above_max);
    RUN_TEST(test_bar_leds_clamped_below_zero);

    RUN_TEST(test_color_tier_min_is_blue);
    RUN_TEST(test_color_tier_low_boundary_is_blue);
    RUN_TEST(test_color_tier_above_low_is_teal);
    RUN_TEST(test_color_tier_mid_boundary_is_teal);
    RUN_TEST(test_color_tier_above_mid_is_magenta);
    RUN_TEST(test_color_tier_max_is_magenta);

    RUN_TEST(test_strobe_off_below_100);
    RUN_TEST(test_strobe_off_at_50);
    RUN_TEST(test_strobe_on_at_100);
    RUN_TEST(test_strobe_on_above_100);

    RUN_TEST(test_9pct_is_1_led_blue);
    RUN_TEST(test_50pct_is_5_leds_teal);
    RUN_TEST(test_100pct_is_10_leds_magenta_with_strobe);

    return UNITY_END();
}
