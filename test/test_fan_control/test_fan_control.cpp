#ifdef UNIT_TEST
    #include <unity.h>
#else
    #include <unity.h>
    #include <Arduino.h>
#endif

#include "../../src/control/FanControl.h"

// Test fixture
FanControl* fan;

void setUp(void) {
    fan = new FanControl(2);  // Use GPIO 2 for testing
}

void tearDown(void) {
    delete fan;
}

// ==================== Initialization Tests ====================

void test_fan_starts_not_running() {
    TEST_ASSERT_FALSE(fan->isRunning());
}

void test_fan_constructor_initializes_state() {
    // Constructor should set running to false
    TEST_ASSERT_FALSE(fan->isRunning());
}

// ==================== Start/Stop Tests ====================

void test_fan_starts() {
    fan->start();
    TEST_ASSERT_TRUE(fan->isRunning());
}

void test_fan_stops() {
    fan->start();
    TEST_ASSERT_TRUE(fan->isRunning());

    fan->stop();
    TEST_ASSERT_FALSE(fan->isRunning());
}

void test_fan_stop_when_not_running() {
    // Should handle gracefully
    fan->stop();
    TEST_ASSERT_FALSE(fan->isRunning());
}

void test_fan_start_when_already_running() {
    fan->start();
    TEST_ASSERT_TRUE(fan->isRunning());

    // Start again - should remain running
    fan->start();
    TEST_ASSERT_TRUE(fan->isRunning());
}

// ==================== State Transition Tests ====================

void test_fan_multiple_start_stop_cycles() {
    // Cycle 1
    fan->start();
    TEST_ASSERT_TRUE(fan->isRunning());
    fan->stop();
    TEST_ASSERT_FALSE(fan->isRunning());

    // Cycle 2
    fan->start();
    TEST_ASSERT_TRUE(fan->isRunning());
    fan->stop();
    TEST_ASSERT_FALSE(fan->isRunning());

    // Cycle 3
    fan->start();
    TEST_ASSERT_TRUE(fan->isRunning());
}

void test_fan_maintains_state() {
    fan->start();

    // Check state multiple times
    TEST_ASSERT_TRUE(fan->isRunning());
    TEST_ASSERT_TRUE(fan->isRunning());
    TEST_ASSERT_TRUE(fan->isRunning());

    fan->stop();

    TEST_ASSERT_FALSE(fan->isRunning());
    TEST_ASSERT_FALSE(fan->isRunning());
}

// ==================== Main Test Runner ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_fan_starts_not_running);
    RUN_TEST(test_fan_constructor_initializes_state);

    // Start/Stop
    RUN_TEST(test_fan_starts);
    RUN_TEST(test_fan_stops);
    RUN_TEST(test_fan_stop_when_not_running);
    RUN_TEST(test_fan_start_when_already_running);

    // State transitions
    RUN_TEST(test_fan_multiple_start_stop_cycles);
    RUN_TEST(test_fan_maintains_state);

    return UNITY_END();
}