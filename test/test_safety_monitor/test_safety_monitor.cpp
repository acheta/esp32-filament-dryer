#ifdef UNIT_TEST
    #include <unity.h>
    #include "../../test/mocks/arduino_mock.h"
#else
    #include <unity.h>
    #include <Arduino.h>
#endif

#include "../../src/control/SafetyMonitor.h"

SafetyMonitor* safety;

void setUp(void) {
    safety = new SafetyMonitor();
}

void tearDown(void) {
    delete safety;
}

// ==================== Initialization Tests ====================

void test_safety_monitor_initializes() {
    safety->begin();
    // No crash = success
    TEST_ASSERT_TRUE(true);
}

// ==================== Temperature Limit Tests ====================

void test_safety_monitor_triggers_on_heater_temp_exceeded() {
    bool emergencyTriggered = false;
    String emergencyReason = "";

    safety->begin();
    safety->setMaxHeaterTemp(90.0);

    safety->registerEmergencyStopCallback(
        [&emergencyTriggered, &emergencyReason](const String& reason) {
            emergencyTriggered = true;
            emergencyReason = reason;
        }
    );

    // Send normal reading
    safety->notifyHeaterTemp(85.0, 1000);
    TEST_ASSERT_FALSE(emergencyTriggered);

    // Exceed limit
    safety->notifyHeaterTemp(91.0, 2000);
    TEST_ASSERT_TRUE(emergencyTriggered);
    TEST_ASSERT_TRUE(emergencyReason.indexOf("Heater") >= 0);
    TEST_ASSERT_TRUE(emergencyReason.indexOf("91") >= 0);
}

void test_safety_monitor_triggers_on_box_temp_exceeded() {
    bool emergencyTriggered = false;
    String emergencyReason = "";

    safety->begin();
    safety->setMaxBoxTemp(80.0);

    safety->registerEmergencyStopCallback(
        [&emergencyTriggered, &emergencyReason](const String& reason) {
            emergencyTriggered = true;
            emergencyReason = reason;
        }
    );

    // Send normal reading
    safety->notifyBoxTemp(75.0, 1000);
    TEST_ASSERT_FALSE(emergencyTriggered);

    // Exceed limit
    safety->notifyBoxTemp(82.0, 2000);
    TEST_ASSERT_TRUE(emergencyTriggered);
    TEST_ASSERT_TRUE(emergencyReason.indexOf("Box") >= 0);
}

void test_safety_monitor_accepts_temp_at_limit() {
    bool emergencyTriggered = false;

    safety->begin();
    safety->setMaxHeaterTemp(90.0);

    safety->registerEmergencyStopCallback(
        [&emergencyTriggered](const String& reason) {
            emergencyTriggered = true;
        }
    );

    // Exactly at limit should trigger (>= check)
    safety->notifyHeaterTemp(90.0, 1000);
    TEST_ASSERT_TRUE(emergencyTriggered);
}

void test_safety_monitor_allows_temp_just_below_limit() {
    bool emergencyTriggered = false;

    safety->begin();
    safety->setMaxHeaterTemp(90.0);

    safety->registerEmergencyStopCallback(
        [&emergencyTriggered](const String& reason) {
            emergencyTriggered = true;
        }
    );

    safety->notifyHeaterTemp(89.9, 1000);
    TEST_ASSERT_FALSE(emergencyTriggered);
}

// ==================== Timeout Tests ====================

void test_safety_monitor_triggers_on_heater_sensor_timeout() {
    bool emergencyTriggered = false;
    String emergencyReason = "";

    safety->begin();

    safety->registerEmergencyStopCallback(
        [&emergencyTriggered, &emergencyReason](const String& reason) {
            emergencyTriggered = true;
            emergencyReason = reason;
        }
    );

    // Initial reading
    safety->notifyHeaterTemp(60.0, 1000);
    safety->update(2000);
    TEST_ASSERT_FALSE(emergencyTriggered);

    // Timeout (> 5000ms since last reading)
    safety->update(7000);
    TEST_ASSERT_TRUE(emergencyTriggered);
    TEST_ASSERT_TRUE(emergencyReason.indexOf("timeout") >= 0);
}

void test_safety_monitor_triggers_on_box_sensor_timeout() {
    bool emergencyTriggered = false;
    String emergencyReason = "";

    safety->begin();

    safety->registerEmergencyStopCallback(
        [&emergencyTriggered, &emergencyReason](const String& reason) {
            emergencyTriggered = true;
            emergencyReason = reason;
        }
    );

    // Initial reading
    safety->notifyBoxTemp(50.0, 1000);
    safety->update(2000);
    TEST_ASSERT_FALSE(emergencyTriggered);

    // Timeout
    safety->update(7000);
    TEST_ASSERT_TRUE(emergencyTriggered);
}

void test_safety_monitor_no_timeout_if_never_had_valid_reading() {
    bool emergencyTriggered = false;

    safety->begin();

    safety->registerEmergencyStopCallback(
        [&emergencyTriggered](const String& reason) {
            emergencyTriggered = true;
        }
    );

    // No readings sent, just update
    safety->update(10000);

    // Should not trigger (sensor was never valid)
    TEST_ASSERT_FALSE(emergencyTriggered);
}

void test_safety_monitor_resets_timeout_on_new_reading() {
    bool emergencyTriggered = false;

    safety->begin();

    safety->registerEmergencyStopCallback(
        [&emergencyTriggered](const String& reason) {
            emergencyTriggered = true;
        }
    );

    // Reading at 1000ms
    safety->notifyHeaterTemp(60.0, 1000);

    // Update at 4000ms (3 seconds elapsed, no timeout)
    safety->update(4000);
    TEST_ASSERT_FALSE(emergencyTriggered);

    // New reading at 5000ms (resets timeout)
    safety->notifyHeaterTemp(62.0, 5000);

    // Update at 9000ms (4 seconds since last reading, no timeout)
    safety->update(9000);
    TEST_ASSERT_FALSE(emergencyTriggered);

    // Update at 11000ms (6 seconds since last reading, timeout)
    safety->update(11000);
    TEST_ASSERT_TRUE(emergencyTriggered);
}

// ==================== Multiple Callback Tests ====================

void test_safety_monitor_notifies_all_registered_callbacks() {
    int callback1Count = 0;
    int callback2Count = 0;
    int callback3Count = 0;

    safety->begin();
    safety->setMaxHeaterTemp(90.0);

    safety->registerEmergencyStopCallback(
        [&callback1Count](const String& reason) { callback1Count++; }
    );

    safety->registerEmergencyStopCallback(
        [&callback2Count](const String& reason) { callback2Count++; }
    );

    safety->registerEmergencyStopCallback(
        [&callback3Count](const String& reason) { callback3Count++; }
    );

    safety->notifyHeaterTemp(95.0, 1000);

    TEST_ASSERT_EQUAL(1, callback1Count);
    TEST_ASSERT_EQUAL(1, callback2Count);
    TEST_ASSERT_EQUAL(1, callback3Count);
}

// ==================== Limit Configuration Tests ====================

void test_safety_monitor_respects_custom_heater_limit() {
    bool emergencyTriggered = false;

    safety->begin();
    safety->setMaxHeaterTemp(85.0); // Custom limit

    safety->registerEmergencyStopCallback(
        [&emergencyTriggered](const String& reason) {
            emergencyTriggered = true;
        }
    );

    safety->notifyHeaterTemp(84.0, 1000);
    TEST_ASSERT_FALSE(emergencyTriggered);

    safety->notifyHeaterTemp(86.0, 2000);
    TEST_ASSERT_TRUE(emergencyTriggered);
}

void test_safety_monitor_respects_custom_box_limit() {
    bool emergencyTriggered = false;

    safety->begin();
    safety->setMaxBoxTemp(70.0); // Custom limit

    safety->registerEmergencyStopCallback(
        [&emergencyTriggered](const String& reason) {
            emergencyTriggered = true;
        }
    );

    safety->notifyBoxTemp(69.0, 1000);
    TEST_ASSERT_FALSE(emergencyTriggered);

    safety->notifyBoxTemp(71.0, 2000);
    TEST_ASSERT_TRUE(emergencyTriggered);
}

// ==================== Emergency Trigger Once Tests ====================

void test_safety_monitor_triggers_emergency_only_once() {
    int callbackCount = 0;

    safety->begin();
    safety->setMaxHeaterTemp(90.0);

    safety->registerEmergencyStopCallback(
        [&callbackCount](const String& reason) {
            callbackCount++;
        }
    );

    // First violation
    safety->notifyHeaterTemp(95.0, 1000);
    TEST_ASSERT_EQUAL(1, callbackCount);

    // More violations should not trigger again
    safety->notifyHeaterTemp(96.0, 2000);
    safety->notifyHeaterTemp(97.0, 3000);

    TEST_ASSERT_EQUAL(1, callbackCount);
}

// ==================== Integration Scenario Tests ====================

void test_safety_monitor_normal_operation_sequence() {
    bool emergencyTriggered = false;

    safety->begin();
    safety->setMaxHeaterTemp(90.0);
    safety->setMaxBoxTemp(80.0);

    safety->registerEmergencyStopCallback(
        [&emergencyTriggered](const String& reason) {
            emergencyTriggered = true;
        }
    );

    // Simulate normal heating cycle
    for (uint32_t t = 0; t <= 10000; t += 500) {
        // Heater temp rising gradually
        float heaterTemp = 20.0 + (t / 500.0) * 2.0; // +2°C every 500ms
        safety->notifyHeaterTemp(heaterTemp, t);

        // Box temp rising slower
        if (t % 2000 == 0) {
            float boxTemp = 20.0 + (t / 2000.0) * 3.0; // +3°C every 2000ms
            safety->notifyBoxTemp(boxTemp, t);
        }

        safety->update(t);
    }

    // Should complete without emergency
    TEST_ASSERT_FALSE(emergencyTriggered);
}

void test_safety_monitor_handles_both_sensors_timing_out() {
    int emergencyCount = 0;

    safety->begin();

    safety->registerEmergencyStopCallback(
        [&emergencyCount](const String& reason) {
            emergencyCount++;
        }
    );

    // Initial readings
    safety->notifyHeaterTemp(60.0, 1000);
    safety->notifyBoxTemp(50.0, 1000);

    // Both timeout (first one to trigger wins)
    safety->update(7000);

    // Only one emergency should trigger
    TEST_ASSERT_EQUAL(1, emergencyCount);
}

// ==================== Main Test Runner ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_safety_monitor_initializes);

    // Temperature limits
    RUN_TEST(test_safety_monitor_triggers_on_heater_temp_exceeded);
    RUN_TEST(test_safety_monitor_triggers_on_box_temp_exceeded);
    RUN_TEST(test_safety_monitor_accepts_temp_at_limit);
    RUN_TEST(test_safety_monitor_allows_temp_just_below_limit);

    // Timeouts
    RUN_TEST(test_safety_monitor_triggers_on_heater_sensor_timeout);
    RUN_TEST(test_safety_monitor_triggers_on_box_sensor_timeout);
    RUN_TEST(test_safety_monitor_no_timeout_if_never_had_valid_reading);
    RUN_TEST(test_safety_monitor_resets_timeout_on_new_reading);

    // Multiple callbacks
    RUN_TEST(test_safety_monitor_notifies_all_registered_callbacks);

    // Configuration
    RUN_TEST(test_safety_monitor_respects_custom_heater_limit);
    RUN_TEST(test_safety_monitor_respects_custom_box_limit);

    // Emergency trigger once
    RUN_TEST(test_safety_monitor_triggers_emergency_only_once);

    // Integration scenarios
    RUN_TEST(test_safety_monitor_normal_operation_sequence);
    RUN_TEST(test_safety_monitor_handles_both_sensors_timing_out);

    return UNITY_END();
}