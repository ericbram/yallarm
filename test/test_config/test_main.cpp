#include <unity.h>
#include "config.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// LED geometry — catches edits to one constant without updating the others
// ---------------------------------------------------------------------------

void test_led_bar_count_matches_indices(void) {
    // LED_BAR_END - LED_BAR_START + 1 must equal LED_BAR_COUNT
    TEST_ASSERT_EQUAL(LED_BAR_COUNT, LED_BAR_END - LED_BAR_START + 1);
}

void test_led_total_count_matches_segments(void) {
    // Logo segment + bar segment must account for all LEDs
    int logo_count = LED_LOGO_END - LED_LOGO_START + 1;
    TEST_ASSERT_EQUAL(NUM_LEDS, logo_count + LED_BAR_COUNT);
}

void test_led_segments_are_contiguous(void) {
    // Bar starts immediately after logo — no gap, no overlap
    TEST_ASSERT_EQUAL(LED_LOGO_END + 1, LED_BAR_START);
}

void test_led_bar_end_within_strip(void) {
    TEST_ASSERT_LESS_THAN(NUM_LEDS, LED_BAR_END);
}

void test_led_logo_start_is_zero(void) {
    TEST_ASSERT_EQUAL(0, LED_LOGO_START);
}

// ---------------------------------------------------------------------------
// Color tier ordering — tier boundaries must be in ascending order
// ---------------------------------------------------------------------------

void test_color_tier_low_before_mid(void) {
    TEST_ASSERT_LESS_THAN(COLOR_TIER_MID, COLOR_TIER_LOW);
}

void test_color_tier_low_in_valid_range(void) {
    TEST_ASSERT_GREATER_THAN(0, COLOR_TIER_LOW);
    TEST_ASSERT_LESS_THAN(100, COLOR_TIER_LOW);
}

void test_color_tier_mid_in_valid_range(void) {
    TEST_ASSERT_GREATER_THAN(0, COLOR_TIER_MID);
    TEST_ASSERT_LESS_THAN(100, COLOR_TIER_MID);
}

// ---------------------------------------------------------------------------
// LED visual settings — values must be within hardware-enforced ranges
// ---------------------------------------------------------------------------

void test_led_brightness_in_range(void) {
    TEST_ASSERT_GREATER_THAN(0, LED_BRIGHTNESS);
    TEST_ASSERT_LESS_OR_EQUAL(255, LED_BRIGHTNESS);
}

void test_breath_min_less_than_max(void) {
    TEST_ASSERT_LESS_THAN(BREATH_MAX, BREATH_MIN);
}

void test_breath_min_non_negative(void) {
    TEST_ASSERT_GREATER_OR_EQUAL(0, BREATH_MIN);
}

void test_breath_max_in_brightness_range(void) {
    TEST_ASSERT_LESS_OR_EQUAL(LED_BRIGHTNESS, BREATH_MAX);
}

// ---------------------------------------------------------------------------
// Audio — MAX98357A volume range is 0–21
// ---------------------------------------------------------------------------

void test_audio_volume_in_range(void) {
    TEST_ASSERT_GREATER_OR_EQUAL(0, AUDIO_VOLUME);
    TEST_ASSERT_LESS_OR_EQUAL(21, AUDIO_VOLUME);
}

// ---------------------------------------------------------------------------
// WiFi — captive portal timeout must be a positive value
// ---------------------------------------------------------------------------

void test_wifi_ap_timeout_positive(void) {
    TEST_ASSERT_GREATER_THAN(0, WIFI_AP_TIMEOUT_S);
}

// ---------------------------------------------------------------------------
// WIS polling — interval and floor must be positive
// ---------------------------------------------------------------------------

void test_wis_poll_interval_positive(void) {
    TEST_ASSERT_GREATER_THAN(0, WIS_POLL_INTERVAL_MS);
}

void test_wis_threshold_floor_positive(void) {
    TEST_ASSERT_GREATER_THAN(0.0f, WIS_THRESHOLD_FLOOR);
}

// ---------------------------------------------------------------------------
// Dismiss button debounce — must be non-zero to be effective
// ---------------------------------------------------------------------------

void test_dismiss_debounce_positive(void) {
    TEST_ASSERT_GREATER_THAN(0, DISMISS_DEBOUNCE_MS);
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_led_bar_count_matches_indices);
    RUN_TEST(test_led_total_count_matches_segments);
    RUN_TEST(test_led_segments_are_contiguous);
    RUN_TEST(test_led_bar_end_within_strip);
    RUN_TEST(test_led_logo_start_is_zero);

    RUN_TEST(test_color_tier_low_before_mid);
    RUN_TEST(test_color_tier_low_in_valid_range);
    RUN_TEST(test_color_tier_mid_in_valid_range);

    RUN_TEST(test_led_brightness_in_range);
    RUN_TEST(test_breath_min_less_than_max);
    RUN_TEST(test_breath_min_non_negative);
    RUN_TEST(test_breath_max_in_brightness_range);

    RUN_TEST(test_audio_volume_in_range);

    RUN_TEST(test_wifi_ap_timeout_positive);

    RUN_TEST(test_wis_poll_interval_positive);
    RUN_TEST(test_wis_threshold_floor_positive);

    RUN_TEST(test_dismiss_debounce_positive);

    return UNITY_END();
}
