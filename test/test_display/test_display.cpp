#ifdef UNIT_TEST
    #include <unity.h>
   // #include "mocks/arduino_mock.h"
#else
    #include <unity.h>
    #include <Arduino.h>
#endif

#include "mocks/MockDisplay.h"

// Test fixture
MockDisplay* display;

void setUp(void) {
    display = new MockDisplay();
}

void tearDown(void) {
    delete display;
}

// ==================== Initialization Tests ====================

void test_display_initializes() {
    display->begin();
    TEST_ASSERT_TRUE(display->isInitialized());
}

// ==================== Clear and Display Tests ====================

void test_display_clear_increments_counter() {
    display->clear();
    TEST_ASSERT_EQUAL(1, display->getClearCallCount());

    display->clear();
    TEST_ASSERT_EQUAL(2, display->getClearCallCount());
}

void test_display_display_increments_counter() {
    display->display();
    TEST_ASSERT_EQUAL(1, display->getDisplayCallCount());

    display->display();
    TEST_ASSERT_EQUAL(2, display->getDisplayCallCount());
}

// ==================== Sensor Readings Display Tests ====================

void test_display_shows_valid_sensor_readings() {
    display->showSensorReadings(65.5, true, 48.2, 35.0, true);

    TEST_ASSERT_EQUAL(1, display->getShowSensorReadingsCallCount());
    TEST_ASSERT_EQUAL_FLOAT(65.5, display->getLastHeaterTemp());
    TEST_ASSERT_TRUE(display->getLastHeaterValid());
    TEST_ASSERT_EQUAL_FLOAT(48.2, display->getLastBoxTemp());
    TEST_ASSERT_EQUAL_FLOAT(35.0, display->getLastBoxHumidity());
    TEST_ASSERT_TRUE(display->getLastBoxValid());
}

void test_display_shows_invalid_heater_reading() {
    display->showSensorReadings(0.0, false, 48.2, 35.0, true);

    TEST_ASSERT_EQUAL(1, display->getShowSensorReadingsCallCount());
    TEST_ASSERT_FALSE(display->getLastHeaterValid());
    TEST_ASSERT_TRUE(display->getLastBoxValid());
}

void test_display_shows_invalid_box_reading() {
    display->showSensorReadings(65.5, true, 0.0, 0.0, false);

    TEST_ASSERT_EQUAL(1, display->getShowSensorReadingsCallCount());
    TEST_ASSERT_TRUE(display->getLastHeaterValid());
    TEST_ASSERT_FALSE(display->getLastBoxValid());
}

void test_display_shows_all_invalid_readings() {
    display->showSensorReadings(0.0, false, 0.0, 0.0, false);

    TEST_ASSERT_EQUAL(1, display->getShowSensorReadingsCallCount());
    TEST_ASSERT_FALSE(display->getLastHeaterValid());
    TEST_ASSERT_FALSE(display->getLastBoxValid());
}

void test_display_updates_readings_multiple_times() {
    display->showSensorReadings(60.0, true, 45.0, 30.0, true);
    TEST_ASSERT_EQUAL_FLOAT(60.0, display->getLastHeaterTemp());

    display->showSensorReadings(70.0, true, 50.0, 40.0, true);
    TEST_ASSERT_EQUAL_FLOAT(70.0, display->getLastHeaterTemp());
    TEST_ASSERT_EQUAL_FLOAT(50.0, display->getLastBoxTemp());

    TEST_ASSERT_EQUAL(2, display->getShowSensorReadingsCallCount());
}

// ==================== Low-Level Drawing Tests ====================

void test_display_set_cursor() {
    display->setCursor(10, 20);
    // Mock doesn't throw, verify no crash
    TEST_ASSERT_TRUE(true);
}

void test_display_set_text_size() {
    display->setTextSize(2);
    // Mock doesn't throw, verify no crash
    TEST_ASSERT_TRUE(true);
}

void test_display_print_text() {
    display->print("Test");
    TEST_ASSERT_EQUAL(1, display->getTextCommandCount());
    TEST_ASSERT_EQUAL_STRING("Test", display->getTextAtIndex(0).c_str());
}

void test_display_println_text() {
    display->println("Line1");
    display->println("Line2");

    TEST_ASSERT_EQUAL(2, display->getTextCommandCount());
    TEST_ASSERT_EQUAL_STRING("Line1", display->getTextAtIndex(0).c_str());
    TEST_ASSERT_EQUAL_STRING("Line2", display->getTextAtIndex(1).c_str());
}

void test_display_clear_resets_text_commands() {
    display->print("Text1");
    display->print("Text2");
    TEST_ASSERT_EQUAL(2, display->getTextCommandCount());

    display->clear();
    TEST_ASSERT_EQUAL(0, display->getTextCommandCount());
}

// ==================== Reset Tests ====================

void test_display_reset_counts() {
    display->clear();
    display->display();
    display->showSensorReadings(50.0, true, 40.0, 30.0, true);

    TEST_ASSERT_EQUAL(1, display->getClearCallCount());
    TEST_ASSERT_EQUAL(1, display->getDisplayCallCount());
    TEST_ASSERT_EQUAL(1, display->getShowSensorReadingsCallCount());

    display->resetCounts();

    TEST_ASSERT_EQUAL(0, display->getClearCallCount());
    TEST_ASSERT_EQUAL(0, display->getDisplayCallCount());
    TEST_ASSERT_EQUAL(0, display->getShowSensorReadingsCallCount());
}

// ==================== Integration Scenario Tests ====================

void test_display_typical_update_sequence() {
    // Simulate typical display update cycle
    display->begin();

    // First update
    display->showSensorReadings(55.0, true, 42.0, 32.0, true);
    TEST_ASSERT_EQUAL(1, display->getShowSensorReadingsCallCount());

    // Second update
    display->showSensorReadings(56.5, true, 43.0, 33.0, true);
    TEST_ASSERT_EQUAL(2, display->getShowSensorReadingsCallCount());
    TEST_ASSERT_EQUAL_FLOAT(56.5, display->getLastHeaterTemp());
}

void test_display_sensor_failure_sequence() {
    display->begin();

    // Start with valid readings
    display->showSensorReadings(60.0, true, 45.0, 35.0, true);
    TEST_ASSERT_TRUE(display->getLastHeaterValid());
    TEST_ASSERT_TRUE(display->getLastBoxValid());

    // Heater fails
    display->showSensorReadings(0.0, false, 45.0, 35.0, true);
    TEST_ASSERT_FALSE(display->getLastHeaterValid());
    TEST_ASSERT_TRUE(display->getLastBoxValid());

    // Heater recovers
    display->showSensorReadings(60.0, true, 45.0, 35.0, true);
    TEST_ASSERT_TRUE(display->getLastHeaterValid());
    TEST_ASSERT_TRUE(display->getLastBoxValid());
}

// ==================== Main Test Runner ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_display_initializes);

    // Basic operations
    RUN_TEST(test_display_clear_increments_counter);
    RUN_TEST(test_display_display_increments_counter);

    // Sensor readings display
    RUN_TEST(test_display_shows_valid_sensor_readings);
    RUN_TEST(test_display_shows_invalid_heater_reading);
    RUN_TEST(test_display_shows_invalid_box_reading);
    RUN_TEST(test_display_shows_all_invalid_readings);
    RUN_TEST(test_display_updates_readings_multiple_times);

    // Low-level drawing
    RUN_TEST(test_display_set_cursor);
    RUN_TEST(test_display_set_text_size);
    RUN_TEST(test_display_print_text);
    RUN_TEST(test_display_println_text);
    RUN_TEST(test_display_clear_resets_text_commands);

    // Reset
    RUN_TEST(test_display_reset_counts);

    // Integration scenarios
    RUN_TEST(test_display_typical_update_sequence);
    RUN_TEST(test_display_sensor_failure_sequence);

    return UNITY_END();
}