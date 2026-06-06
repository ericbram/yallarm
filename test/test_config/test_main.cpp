#include <unity.h>
#include "config.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// LED geometry — two physically separate strips, each on its own GPIO
// ---------------------------------------------------------------------------

void test_led_logo_count_positive(void) {
    TEST_ASSERT_GREATER_THAN(0, LED_LOGO_COUNT);
}

void test_led_bar_count_positive(void) {
    TEST_ASSERT_GREATER_THAN(0, LED_BAR_COUNT);
}

void test_led_strips_use_distinct_pins(void) {
    // Each strip is its own daisy-chain — they can't share a data pin
    TEST_ASSERT_NOT_EQUAL(LED_LOGO_PIN, LED_BAR_PIN);
}

void test_led_pins_avoid_strapping_pins(void) {
    // ESP32-S3 strapping pins (0, 3, 45, 46) affect boot mode — driving a
    // WS2812B data line from one can wedge the board on power cycle.
    const int strapping[] = {0, 3, 45, 46};
    for (unsigned i = 0; i < sizeof(strapping) / sizeof(strapping[0]); i++) {
        TEST_ASSERT_NOT_EQUAL(strapping[i], LED_LOGO_PIN);
        TEST_ASSERT_NOT_EQUAL(strapping[i], LED_BAR_PIN);
    }
}

// ---------------------------------------------------------------------------
// LED visual settings — values must be within hardware-enforced ranges
// ---------------------------------------------------------------------------

void test_led_brightness_in_range(void) {
    TEST_ASSERT_GREATER_THAN(0, LED_BRIGHTNESS);
    TEST_ASSERT_LESS_OR_EQUAL(255, LED_BRIGHTNESS);
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

    RUN_TEST(test_led_logo_count_positive);
    RUN_TEST(test_led_bar_count_positive);
    RUN_TEST(test_led_strips_use_distinct_pins);
    RUN_TEST(test_led_pins_avoid_strapping_pins);

    RUN_TEST(test_led_brightness_in_range);

    RUN_TEST(test_audio_volume_in_range);

    RUN_TEST(test_wifi_ap_timeout_positive);

    RUN_TEST(test_wis_poll_interval_positive);
    RUN_TEST(test_wis_threshold_floor_positive);

    RUN_TEST(test_dismiss_debounce_positive);

    return UNITY_END();
}
