#include <unity.h>
#include "led_calc.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// computeBarLeds — wis_pct → number of LEDs lit (0–20, 1 LED per 5%)
// ---------------------------------------------------------------------------

void test_bar_leds_at_1pct(void) {
    // round(0.2) = 0
    TEST_ASSERT_EQUAL(0, computeBarLeds(1));
}

void test_bar_leds_at_3pct(void) {
    // round(0.6) = 1
    TEST_ASSERT_EQUAL(1, computeBarLeds(3));
}

void test_bar_leds_at_5pct(void) {
    // round(1.0) = 1
    TEST_ASSERT_EQUAL(1, computeBarLeds(5));
}

void test_bar_leds_at_7pct(void) {
    // round(1.4) = 1
    TEST_ASSERT_EQUAL(1, computeBarLeds(7));
}

void test_bar_leds_at_8pct(void) {
    // round(1.6) = 2
    TEST_ASSERT_EQUAL(2, computeBarLeds(8));
}

void test_bar_leds_at_10pct(void) {
    // round(2.0) = 2
    TEST_ASSERT_EQUAL(2, computeBarLeds(10));
}

void test_bar_leds_at_50pct(void) {
    // round(10.0) = 10 — exactly half of 20
    TEST_ASSERT_EQUAL(10, computeBarLeds(50));
}

void test_bar_leds_at_90pct(void) {
    // round(18.0) = 18
    TEST_ASSERT_EQUAL(18, computeBarLeds(90));
}

void test_bar_leds_at_98pct(void) {
    // round(19.6) = 20
    TEST_ASSERT_EQUAL(20, computeBarLeds(98));
}

void test_bar_leds_at_100pct(void) {
    // round(20.0) = 20 — all LEDs lit
    TEST_ASSERT_EQUAL(20, computeBarLeds(100));
}

void test_bar_leds_clamped_above_max(void) {
    // Over 100 should still return max LED count
    TEST_ASSERT_EQUAL(20, computeBarLeds(150));
}

void test_bar_leds_clamped_below_zero(void) {
    // Negative input should return 0
    TEST_ASSERT_EQUAL(0, computeBarLeds(-10));
}

// ---------------------------------------------------------------------------
// colorForPct — green at 1% → yellow at 50% → red at 100%
// ---------------------------------------------------------------------------

void test_color_at_1pct_is_green(void) {
    RgbColor c = colorForPct(1);
    TEST_ASSERT_EQUAL_UINT8(4,   c.r);
    TEST_ASSERT_EQUAL_UINT8(200, c.g);
    TEST_ASSERT_EQUAL_UINT8(0,   c.b);
}

void test_color_at_25pct_is_yellow_green(void) {
    RgbColor c = colorForPct(25);
    TEST_ASSERT_EQUAL_UINT8(100, c.r);
    TEST_ASSERT_EQUAL_UINT8(200, c.g);
    TEST_ASSERT_EQUAL_UINT8(0,   c.b);
}

void test_color_at_50pct_is_yellow(void) {
    RgbColor c = colorForPct(50);
    TEST_ASSERT_EQUAL_UINT8(200, c.r);
    TEST_ASSERT_EQUAL_UINT8(200, c.g);
    TEST_ASSERT_EQUAL_UINT8(0,   c.b);
}

void test_color_at_75pct_is_orange(void) {
    RgbColor c = colorForPct(75);
    TEST_ASSERT_EQUAL_UINT8(200, c.r);
    TEST_ASSERT_EQUAL_UINT8(100, c.g);
    TEST_ASSERT_EQUAL_UINT8(0,   c.b);
}

void test_color_at_100pct_is_red(void) {
    RgbColor c = colorForPct(100);
    TEST_ASSERT_EQUAL_UINT8(200, c.r);
    TEST_ASSERT_EQUAL_UINT8(0,   c.g);
    TEST_ASSERT_EQUAL_UINT8(0,   c.b);
}

void test_color_clamped_above_max(void) {
    // Over 100 should clamp to red
    RgbColor c = colorForPct(150);
    TEST_ASSERT_EQUAL_UINT8(200, c.r);
    TEST_ASSERT_EQUAL_UINT8(0,   c.g);
    TEST_ASSERT_EQUAL_UINT8(0,   c.b);
}

void test_color_clamped_below_one(void) {
    // Below 1 should clamp to bright-green minimum
    RgbColor c = colorForPct(-10);
    TEST_ASSERT_EQUAL_UINT8(4,   c.r);
    TEST_ASSERT_EQUAL_UINT8(200, c.g);
    TEST_ASSERT_EQUAL_UINT8(0,   c.b);
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
// Combined — verify bar fill + color at key thresholds
// ---------------------------------------------------------------------------

void test_9pct_is_2_leds_green(void) {
    // Today's typical WIS (14.49 / 147.16 ≈ 9%) → round(2.25) = 2
    int leds = computeBarLeds(9);
    RgbColor c = colorForPct(9);
    TEST_ASSERT_EQUAL(2, leds);
    // 9% < 50 → r = 36, g = 200 (still green-leaning)
    TEST_ASSERT_EQUAL_UINT8(36,  c.r);
    TEST_ASSERT_EQUAL_UINT8(200, c.g);
}

void test_50pct_is_10_leds_yellow(void) {
    int leds = computeBarLeds(50);
    RgbColor c = colorForPct(50);
    TEST_ASSERT_EQUAL(10, leds);
    TEST_ASSERT_EQUAL_UINT8(200, c.r);
    TEST_ASSERT_EQUAL_UINT8(200, c.g);
}

void test_100pct_is_20_leds_red_with_strobe(void) {
    int leds = computeBarLeds(100);
    RgbColor c = colorForPct(100);
    bool strobe = computeStrobe(100);
    TEST_ASSERT_EQUAL(20, leds);
    TEST_ASSERT_EQUAL_UINT8(200, c.r);
    TEST_ASSERT_EQUAL_UINT8(0,   c.g);
    TEST_ASSERT_TRUE(strobe);
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_bar_leds_at_1pct);
    RUN_TEST(test_bar_leds_at_3pct);
    RUN_TEST(test_bar_leds_at_5pct);
    RUN_TEST(test_bar_leds_at_7pct);
    RUN_TEST(test_bar_leds_at_8pct);
    RUN_TEST(test_bar_leds_at_10pct);
    RUN_TEST(test_bar_leds_at_50pct);
    RUN_TEST(test_bar_leds_at_90pct);
    RUN_TEST(test_bar_leds_at_98pct);
    RUN_TEST(test_bar_leds_at_100pct);
    RUN_TEST(test_bar_leds_clamped_above_max);
    RUN_TEST(test_bar_leds_clamped_below_zero);

    RUN_TEST(test_color_at_1pct_is_green);
    RUN_TEST(test_color_at_25pct_is_yellow_green);
    RUN_TEST(test_color_at_50pct_is_yellow);
    RUN_TEST(test_color_at_75pct_is_orange);
    RUN_TEST(test_color_at_100pct_is_red);
    RUN_TEST(test_color_clamped_above_max);
    RUN_TEST(test_color_clamped_below_one);

    RUN_TEST(test_strobe_off_below_100);
    RUN_TEST(test_strobe_off_at_50);
    RUN_TEST(test_strobe_on_at_100);
    RUN_TEST(test_strobe_on_above_100);

    RUN_TEST(test_9pct_is_2_leds_green);
    RUN_TEST(test_50pct_is_10_leds_yellow);
    RUN_TEST(test_100pct_is_20_leds_red_with_strobe);

    return UNITY_END();
}
