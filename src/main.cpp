/**
 * Main loop - called continuously
 */

#include <Arduino.h>
#include "Config.h"
#include "Types.h"
#include "ComponentFactory.h"

// Interfaces
#include "interfaces/ISensorManager.h"
#include "interfaces/IHeaterTempSensor.h"
#include "interfaces/IBoxTempHumiditySensor.h"
#include "interfaces/IHeaterControl.h"
#include "interfaces/IPIDController.h"
#include "interfaces/ISafetyMonitor.h"
#include "interfaces/ISettingsStorage.h"
#include "interfaces/ISoundController.h"
#include "interfaces/IDryer.h"
#include "interfaces/IDisplay.h"

// TODO: Include actual implementations when created
// #include "control/HeaterControl.h"
// #include "control/PIDController.h"
// #include "control/SafetyMonitor.h"
// #include "storage/SettingsStorage.h"
// #include "userInterface/SoundController.h"

// Global component pointers
IHeaterTempSensor* heaterSensor = nullptr;
IBoxTempHumiditySensor* boxSensor = nullptr;
ISensorManager* sensorManager = nullptr;
IDisplay* oledDisplay = nullptr;
IHeaterControl* heaterControl = nullptr;
IPIDController* pidController = nullptr;
ISafetyMonitor* safetyMonitor = nullptr;
ISettingsStorage* settingsStorage = nullptr;
ISoundController* soundController = nullptr;
IDryer* dryer = nullptr;

// Factory pointers
IHeaterTempSensorFactory* heaterSensorFactory = nullptr;
IBoxTempHumiditySensorFactory* boxSensorFactory = nullptr;
ISensorManagerFactory* sensorManagerFactory = nullptr;
IDisplayFactory* displayFactory = nullptr;
IDryerFactory* dryerFactory = nullptr;

// Display update timing
uint32_t lastDisplayUpdate = 0;

/**
 * Initialize all hardware and create component instances using factories
 */
void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    Serial.println("\n\n========================================");
    Serial.println("ESP32 Dryer Initializing...");
    Serial.println("========================================\n");

    // ==================== Create Factories ====================
    Serial.println("Creating component factories...");

    heaterSensorFactory = ComponentFactoryProvider::getHeaterTempSensorFactory();
    Serial.println("  - HeaterTempSensor factory created");

    boxSensorFactory = ComponentFactoryProvider::getBoxTempHumiditySensorFactory();
    Serial.println("  - BoxTempHumiditySensor factory created");

    sensorManagerFactory = ComponentFactoryProvider::getSensorManagerFactory();
    Serial.println("  - SensorManager factory created");

    displayFactory = ComponentFactoryProvider::getDisplayFactory();
    Serial.println("  - Display factory created");

    dryerFactory = ComponentFactoryProvider::getDryerFactory();
    Serial.println("  - Dryer factory created");

    // ==================== Create Sensor Objects Using Factories ====================
    Serial.println("\nCreating sensor objects from factories...");

    // Create heater temperature sensor (DS18B20)
    heaterSensor = heaterSensorFactory->create();
    Serial.println("  - Heater temperature sensor created");

    // Create box temperature/humidity sensor (AM2320)
    boxSensor = boxSensorFactory->create();
    Serial.println("  - Box temp/humidity sensor created");

    // Create sensor manager with injected sensors
    // Note: For production, SensorManager factory creates its own sensors internally
    // For maximum flexibility, we could modify the factory to accept sensors as parameters
    sensorManager = new SensorManager(heaterSensor, boxSensor);
    Serial.println("  - SensorManager created with injected sensors");

    // Create OLED display
    oledDisplay = displayFactory->create();
    Serial.println("  - OLED Display created");

    // ==================== Create Control Components ====================
    Serial.println("\nCreating control components...");

    // TODO: Create factories and use them when components are implemented
    // IHeaterControlFactory* heaterControlFactory = ComponentFactoryProvider::getHeaterControlFactory();
    // heaterControl = heaterControlFactory->create();
    // Serial.println("  - HeaterControl created");

    // IPIDControllerFactory* pidControllerFactory = ComponentFactoryProvider::getPIDControllerFactory();
    // pidController = pidControllerFactory->create();
    // Serial.println("  - PIDController created");

    // ISafetyMonitorFactory* safetyMonitorFactory = ComponentFactoryProvider::getSafetyMonitorFactory();
    // safetyMonitor = safetyMonitorFactory->create();
    // Serial.println("  - SafetyMonitor created");

    // ==================== Create Storage Components ====================
    Serial.println("\nCreating storage components...");

    // TODO: Create factories and use them when implemented
    // ISettingsStorageFactory* storageFactory = ComponentFactoryProvider::getSettingsStorageFactory();
    // settingsStorage = storageFactory->create();
    // Serial.println("  - SettingsStorage created");

    // ==================== Create UI Components ====================
    Serial.println("\nCreating UI components...");

    // TODO: Create factories and use them when implemented
    // ISoundControllerFactory* soundFactory = ComponentFactoryProvider::getSoundControllerFactory();
    // soundController = soundFactory->create();
    // Serial.println("  - SoundController created");

    // ==================== Create Main Dryer Orchestrator Using Factory ====================
    Serial.println("\nCreating Dryer orchestrator from factory...");

    // TODO: Uncomment when all components are implemented
    /*
    dryer = dryerFactory->create(
        sensorManager,
        heaterControl,
        pidController,
        safetyMonitor,
        settingsStorage,
        soundController
    );
    Serial.println("  - Dryer created via factory");
    */

    // ==================== Initialize All Components ====================
    Serial.println("\n========================================");
    Serial.println("Initializing components...");
    Serial.println("========================================\n");

    // Initialize sensor manager (which initializes sensors)
    sensorManager->begin();
    Serial.println("  ✓ SensorManager initialized");

    // Initialize OLED display
    oledDisplay->begin();
    Serial.println("  ✓ OLED Display initialized");

    // Show startup message on display
    oledDisplay->clear();
    oledDisplay->setCursor(0, 0);
    oledDisplay->setTextSize(1);
    oledDisplay->println("Dryer Starting...");
    oledDisplay->display();
    delay(1000);  // Show startup message briefly

    // TODO: Initialize other components when created
    /*
    heaterControl->begin();
    Serial.println("  ✓ HeaterControl initialized");

    pidController->begin();
    Serial.println("  ✓ PIDController initialized");

    safetyMonitor->begin();
    Serial.println("  ✓ SafetyMonitor initialized");

    settingsStorage->begin();
    Serial.println("  ✓ SettingsStorage initialized");

    soundController->begin();
    Serial.println("  ✓ SoundController initialized");

    // Initialize dryer (sets up callbacks, loads settings, etc.)
    dryer->begin();
    Serial.println("  ✓ Dryer initialized");
    */

    // ==================== Register Test Callbacks ====================
    Serial.println("\nRegistering sensor callbacks...");

    // Register callback to print heater temperature
    sensorManager->registerHeaterTempCallback(
        [](float temp, uint32_t timestamp) {
            Serial.print("[");
            Serial.print(timestamp);
            Serial.print("ms] Heater: ");
            Serial.print(temp, 1);
            Serial.println("°C");
        }
    );

    // Register callback to print box data
    sensorManager->registerBoxDataCallback(
        [](float temp, float humidity, uint32_t timestamp) {
            Serial.print("[");
            Serial.print(timestamp);
            Serial.print("ms] Box: ");
            Serial.print(temp, 1);
            Serial.print("°C, ");
            Serial.print(humidity, 1);
            Serial.println("%RH");
        }
    );

    // Register callback to print sensor errors
    sensorManager->registerSensorErrorCallback(
        [](SensorType type, const String& error) {
            Serial.print("⚠️  SENSOR ERROR - ");
            switch(type) {
                case SensorType::HEATER_TEMP:
                    Serial.print("Heater Temp: ");
                    break;
                case SensorType::BOX_TEMP:
                    Serial.print("Box Temp: ");
                    break;
                case SensorType::BOX_HUMIDITY:
                    Serial.print("Box Humidity: ");
                    break;
                default:
                    Serial.print("Unknown: ");
                    break;
            }
            Serial.println(error);
        }
    );

    Serial.println("  ✓ Callbacks registered");

    // ==================== Startup Complete ====================
    Serial.println("\n========================================");
    Serial.println("✓ ESP32 Dryer Ready!");
    Serial.println("========================================\n");

    // Display initial sensor status
    Serial.println("Initial Sensor Status:");
    Serial.print("  Heater Sensor: ");
    Serial.println(sensorManager->isHeaterTempValid() ? "✓ Valid" : "✗ Invalid");
    Serial.print("  Box Sensor: ");
    Serial.println(sensorManager->isBoxDataValid() ? "✓ Valid" : "✗ Invalid");

    if (sensorManager->isHeaterTempValid()) {
        Serial.print("  Heater Temp: ");
        Serial.print(sensorManager->getHeaterTemp(), 1);
        Serial.println("°C");
    }

    if (sensorManager->isBoxDataValid()) {
        Serial.print("  Box Temp: ");
        Serial.print(sensorManager->getBoxTemp(), 1);
        Serial.println("°C");
        Serial.print("  Box Humidity: ");
        Serial.print(sensorManager->getBoxHumidity(), 1);
        Serial.println("%RH");
    }

    Serial.println("\nStarting main loop...\n");
}

void loop() {
    uint32_t currentMillis = millis();

    // Update sensor manager (reads sensors at appropriate intervals)
    sensorManager->update(currentMillis);

    // Update display periodically (every 200ms)
    if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        lastDisplayUpdate = currentMillis;

        // Get current sensor readings
        float heaterTemp = sensorManager->getHeaterTemp();
        bool heaterValid = sensorManager->isHeaterTempValid();
        float boxTemp = sensorManager->getBoxTemp();
        float boxHumidity = sensorManager->getBoxHumidity();
        bool boxValid = sensorManager->isBoxDataValid();

        // Update display with current readings
        oledDisplay->showSensorReadings(heaterTemp, heaterValid,
                                        boxTemp, boxHumidity, boxValid);
    }

    // TODO: Update other components when implemented
    /*
    // Update safety monitor
    safetyMonitor->update(currentMillis);

    // Update dryer (state machine, timing, etc.)
    dryer->update(currentMillis);

    // Update UI components would be in UIController
    */

    // Small delay to prevent overwhelming the system
    // In production, this would be handled by UI refresh rates
    delay(10);
}