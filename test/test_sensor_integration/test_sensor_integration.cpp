#ifdef UNIT_TEST
	#include <unity.h>
	// #include "mocks/arduino_mock.h"
#else
	#include <unity.h>
	#include <Arduino.h>
#endif

#include "../src/sensors/SensorManager.h"
#include "mocks/MockHeaterTempSensor.h"
#include "mocks/MockBoxTempHumiditySensor.h"

// Test fixture
MockHeaterTempSensor* heaterSensor;
MockBoxTempHumiditySensor* boxSensor;
SensorManager* sensorManager;

void setUp(void) {
    heaterSensor = new MockHeaterTempSensor();
    boxSensor = new MockBoxTempHumiditySensor();
    sensorManager = new SensorManager(heaterSensor, boxSensor);
}

void tearDown(void) {
    delete sensorManager;
    delete boxSensor;
    delete heaterSensor;
}

// ==================== Initialization Tests ====================

void test_sensor_manager_initializes_both_sensors() {
    sensorManager->begin();

    TEST_ASSERT_TRUE(heaterSensor->isInitialized());
    TEST_ASSERT_TRUE(boxSensor->isInitialized());
}

void test_sensor_manager_starts_with_invalid_readings_when_sensors_invalid() {
    heaterSensor->setInvalid("Disconnected");
    boxSensor->setInvalid("Communication error");

    sensorManager->begin();

    TEST_ASSERT_FALSE(sensorManager->isHeaterTempValid());
    TEST_ASSERT_FALSE(sensorManager->isBoxDataValid());
}

// ==================== Heater Sensor Integration Tests ====================

void test_sensor_manager_reads_heater_temp_at_500ms_interval() {
    heaterSensor->setTemperature(65.5);
    sensorManager->begin();

    // Reset call count after begin
    heaterSensor->resetCallCount();

    // First update at 0ms - should not read yet
    sensorManager->update(0);
    TEST_ASSERT_EQUAL(0, heaterSensor->getReadCallCount());

    // Update at 500ms - should read
    sensorManager->update(500);
    TEST_ASSERT_EQUAL(1, heaterSensor->getReadCallCount());

    // Update at 999ms - should not read
    sensorManager->update(999);
    TEST_ASSERT_EQUAL(1, heaterSensor->getReadCallCount());

    // Update at 1000ms - should read again
    sensorManager->update(1000);
    TEST_ASSERT_EQUAL(2, heaterSensor->getReadCallCount());
}

void test_sensor_manager_caches_heater_temp_reading() {
    heaterSensor->setTemperature(72.3);
    sensorManager->begin();

    sensorManager->update(500);

    TEST_ASSERT_EQUAL_FLOAT(72.3, sensorManager->getHeaterTemp());
    TEST_ASSERT_TRUE(sensorManager->isHeaterTempValid());
}

void test_sensor_manager_fires_callback_on_heater_temp_update() {
    bool callbackFired = false;
    float receivedTemp = 0;
    uint32_t receivedTimestamp = 0;

    heaterSensor->setTemperature(68.9);
    sensorManager->begin();

    sensorManager->registerHeaterTempCallback(
        [&callbackFired, &receivedTemp, &receivedTimestamp]
        (float temp, uint32_t timestamp) {
            callbackFired = true;
            receivedTemp = temp;
            receivedTimestamp = timestamp;
        }
    );

    sensorManager->update(500);

    TEST_ASSERT_TRUE(callbackFired);
    TEST_ASSERT_EQUAL_FLOAT(68.9, receivedTemp);
    TEST_ASSERT_EQUAL(500, receivedTimestamp);
}

void test_sensor_manager_handles_heater_sensor_error() {
    bool errorCallbackFired = false;
    SensorType errorType = SensorType::BOX_TEMP;
    String errorMessage = "";

    heaterSensor->setInvalid("DS18B20 disconnected");
    sensorManager->begin();

    sensorManager->registerSensorErrorCallback(
        [&errorCallbackFired, &errorType, &errorMessage]
        (SensorType type, const String& error) {
            errorCallbackFired = true;
            errorType = type;
            errorMessage = error;
        }
    );

    sensorManager->update(500);

    TEST_ASSERT_TRUE(errorCallbackFired);
    TEST_ASSERT_EQUAL(SensorType::HEATER_TEMP, errorType);
    TEST_ASSERT_EQUAL_STRING("DS18B20 disconnected", errorMessage.c_str());
    TEST_ASSERT_FALSE(sensorManager->isHeaterTempValid());
}

// ==================== Box Sensor Integration Tests ====================

void test_sensor_manager_reads_box_data_at_2000ms_interval() {
    boxSensor->setReadings(45.2, 38.5);
    sensorManager->begin();

    boxSensor->resetCallCount();

    // Update at 0ms - should not read yet
    sensorManager->update(0);
    TEST_ASSERT_EQUAL(0, boxSensor->getReadCallCount());

    // Update at 1999ms - should not read
    sensorManager->update(1999);
    TEST_ASSERT_EQUAL(0, boxSensor->getReadCallCount());

    // Update at 2000ms - should read
    sensorManager->update(2000);
    TEST_ASSERT_EQUAL(1, boxSensor->getReadCallCount());

    // Update at 4000ms - should read again
    sensorManager->update(4000);
    TEST_ASSERT_EQUAL(2, boxSensor->getReadCallCount());
}

void test_sensor_manager_caches_box_readings() {
    boxSensor->setReadings(48.7, 42.3);
    sensorManager->begin();

    sensorManager->update(2000);

    TEST_ASSERT_EQUAL_FLOAT(48.7, sensorManager->getBoxTemp());
    TEST_ASSERT_EQUAL_FLOAT(42.3, sensorManager->getBoxHumidity());
    TEST_ASSERT_TRUE(sensorManager->isBoxDataValid());
}

void test_sensor_manager_fires_callback_on_box_data_update() {
    bool callbackFired = false;
    float receivedTemp = 0;
    float receivedHumidity = 0;
    uint32_t receivedTimestamp = 0;

    boxSensor->setReadings(50.5, 35.8);
    sensorManager->begin();

    sensorManager->registerBoxDataCallback(
        [&callbackFired, &receivedTemp, &receivedHumidity, &receivedTimestamp]
        (float temp, float humidity, uint32_t timestamp) {
            callbackFired = true;
            receivedTemp = temp;
            receivedHumidity = humidity;
            receivedTimestamp = timestamp;
        }
    );

    sensorManager->update(2000);

    TEST_ASSERT_TRUE(callbackFired);
    TEST_ASSERT_EQUAL_FLOAT(50.5, receivedTemp);
    TEST_ASSERT_EQUAL_FLOAT(35.8, receivedHumidity);
    TEST_ASSERT_EQUAL(2000, receivedTimestamp);
}

void test_sensor_manager_handles_box_sensor_error() {
    bool errorCallbackFired = false;
    SensorType errorType = SensorType::HEATER_TEMP;
    String errorMessage = "";

    boxSensor->setInvalid("AM2320 communication error");
    sensorManager->begin();

    sensorManager->registerSensorErrorCallback(
        [&errorCallbackFired, &errorType, &errorMessage]
        (SensorType type, const String& error) {
            errorCallbackFired = true;
            errorType = type;
            errorMessage = error;
        }
    );

    sensorManager->update(2000);

    TEST_ASSERT_TRUE(errorCallbackFired);
    TEST_ASSERT_EQUAL(SensorType::BOX_TEMP, errorType);
    TEST_ASSERT_EQUAL_STRING("AM2320 communication error", errorMessage.c_str());
    TEST_ASSERT_FALSE(sensorManager->isBoxDataValid());
}

// ==================== Multi-rate Coordination Tests ====================

void test_sensor_manager_coordinates_different_update_rates() {
    heaterSensor->setTemperature(60.0);
    boxSensor->setReadings(45.0, 40.0);
    sensorManager->begin();

    heaterSensor->resetCallCount();
    boxSensor->resetCallCount();

    // At 500ms - heater reads, box doesn't
    sensorManager->update(500);
    TEST_ASSERT_EQUAL(1, heaterSensor->getReadCallCount());
    TEST_ASSERT_EQUAL(0, boxSensor->getReadCallCount());

    // At 1000ms - heater reads again, box doesn't
    sensorManager->update(1000);
    TEST_ASSERT_EQUAL(2, heaterSensor->getReadCallCount());
    TEST_ASSERT_EQUAL(0, boxSensor->getReadCallCount());

    // At 2000ms - both read
    sensorManager->update(2000);
    TEST_ASSERT_EQUAL(3, heaterSensor->getReadCallCount());
    TEST_ASSERT_EQUAL(1, boxSensor->getReadCallCount());
}

void test_sensor_manager_maintains_independent_sensor_states() {
    // Heater valid, box invalid
    heaterSensor->setTemperature(70.0);
    boxSensor->setInvalid("Sensor fault");

    sensorManager->begin();
    sensorManager->update(500);   // Heater update
    sensorManager->update(2000);  // Box update

    TEST_ASSERT_TRUE(sensorManager->isHeaterTempValid());
    TEST_ASSERT_FALSE(sensorManager->isBoxDataValid());
    TEST_ASSERT_EQUAL_FLOAT(70.0, sensorManager->getHeaterTemp());
}

// ==================== Timeout Tests ====================
// Note: Timeout detection removed from SensorManager as it's redundant
// (timeout check runs in same update() call that would be missing)
// True watchdog protection should be implemented at a higher level
// (e.g., hardware watchdog timer, separate monitoring task)

// ==================== Complete Integration Test ====================

void test_sensor_manager_full_integration_with_both_sensors() {
    int heaterUpdates = 0;
    int boxUpdates = 0;
    int errors = 0;

    heaterSensor->setTemperature(75.5);
    boxSensor->setReadings(52.3, 38.7);

    sensorManager->begin();

    sensorManager->registerHeaterTempCallback(
        [&heaterUpdates](float temp, uint32_t timestamp) {
            heaterUpdates++;
        }
    );

    sensorManager->registerBoxDataCallback(
        [&boxUpdates](float temp, float humidity, uint32_t timestamp) {
            boxUpdates++;
        }
    );

    sensorManager->registerSensorErrorCallback(
        [&errors](SensorType type, const String& error) {
            errors++;
        }
    );

    // Simulate 3 seconds of updates
    for (uint32_t t = 0; t <= 3000; t += 100) {
        sensorManager->update(t);
    }

    // Heater updates every 500ms: 0, 500, 1000, 1500, 2000, 2500, 3000 = 7 times
    // Box updates every 2000ms: 0, 2000 = 2 times
    TEST_ASSERT_EQUAL(6, heaterUpdates);  // First update at 500ms
    TEST_ASSERT_EQUAL(1, boxUpdates);     // First update at 2000ms
    TEST_ASSERT_EQUAL(0, errors);

    // Verify final cached values
    TEST_ASSERT_EQUAL_FLOAT(75.5, sensorManager->getHeaterTemp());
    TEST_ASSERT_EQUAL_FLOAT(52.3, sensorManager->getBoxTemp());
    TEST_ASSERT_EQUAL_FLOAT(38.7, sensorManager->getBoxHumidity());
}

// ==================== Main Test Runner ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_sensor_manager_initializes_both_sensors);
    RUN_TEST(test_sensor_manager_starts_with_invalid_readings_when_sensors_invalid);

    // Heater sensor integration
    RUN_TEST(test_sensor_manager_reads_heater_temp_at_500ms_interval);
    RUN_TEST(test_sensor_manager_caches_heater_temp_reading);
    RUN_TEST(test_sensor_manager_fires_callback_on_heater_temp_update);
    RUN_TEST(test_sensor_manager_handles_heater_sensor_error);

    // Box sensor integration
    RUN_TEST(test_sensor_manager_reads_box_data_at_2000ms_interval);
    RUN_TEST(test_sensor_manager_caches_box_readings);
    RUN_TEST(test_sensor_manager_fires_callback_on_box_data_update);
    RUN_TEST(test_sensor_manager_handles_box_sensor_error);

    // Multi-rate coordination
    RUN_TEST(test_sensor_manager_coordinates_different_update_rates);
    RUN_TEST(test_sensor_manager_maintains_independent_sensor_states);

    // Full integration
    RUN_TEST(test_sensor_manager_full_integration_with_both_sensors);

    return UNITY_END();
}