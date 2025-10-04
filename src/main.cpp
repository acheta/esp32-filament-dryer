/**
 * ESP32 Dryer - Main Application
 *
 * Integrates all control components with sensor display
 */

#include <Arduino.h>
#include "Config.h"
#include "Types.h"

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

// Implementations
#include "sensors/HeaterTempSensor.h"
#include "sensors/BoxTempHumiditySensor.h"
#include "sensors/SensorManager.h"
#include "control/HeaterControl.h"
#include "control/PIDController.h"
#include "control/SafetyMonitor.h"
#include "userInterface/OLEDDisplay.h"
#include "Dryer.h"

// Global component pointers
IHeaterTempSensor* heaterSensor = nullptr;
IBoxTempHumiditySensor* boxSensor = nullptr;
ISensorManager* sensorManager = nullptr;
IDisplay* oledDisplay = nullptr;
IHeaterControl* heaterControl = nullptr;
IPIDController* pidController = nullptr;
ISafetyMonitor* safetyMonitor = nullptr;
ISoundController* soundController = nullptr;
IDryer* dryer = nullptr;

// Display update timing
uint32_t lastDisplayUpdate = 0;

/**
 * Enhanced display showing both sensor data and dryer status
 */
void updateDisplay() {
    oledDisplay->clear();

    // Get current sensor readings
    float heaterTemp = sensorManager->getHeaterTemp();
    bool heaterValid = sensorManager->isHeaterTempValid();
    float boxTemp = sensorManager->getBoxTemp();
    float boxHumidity = sensorManager->getBoxHumidity();
    bool boxValid = sensorManager->isBoxDataValid();

    // Get dryer stats
    CurrentStats stats = dryer->getCurrentStats();

    // Line 1: State and heater temp
    oledDisplay->setCursor(0, 0);
    oledDisplay->setTextSize(1);

    // Show state
    switch(stats.state) {
        case DryerState::READY:
            oledDisplay->print("RDY ");
            break;
        case DryerState::RUNNING:
            oledDisplay->print("RUN ");
            break;
        case DryerState::PAUSED:
            oledDisplay->print("PAU ");
            break;
        case DryerState::FINISHED:
            oledDisplay->print("FIN ");
            break;
        case DryerState::FAILED:
            oledDisplay->print("ERR ");
            break;
        case DryerState::POWER_RECOVERED:
            oledDisplay->print("PWR ");
            break;
    }

    // Heater temp
    if (heaterValid) {
        oledDisplay->print("H:");
        oledDisplay->print(String(heaterTemp, 1));
        oledDisplay->print("C");
    } else {
        oledDisplay->print("H:--");
    }

    // Target temp (if running)
    if (stats.state == DryerState::RUNNING || stats.state == DryerState::PAUSED) {
        oledDisplay->print("/");
        oledDisplay->print(String(stats.targetTemp, 0));
    }

    // Line 2: Box temp and humidity
    oledDisplay->setCursor(0, 12);
    oledDisplay->print("Box:");
    if (boxValid) {
        oledDisplay->print(String(boxTemp, 1));
        oledDisplay->print("C ");
        oledDisplay->print(String(boxHumidity, 0));
        oledDisplay->print("%");
    } else {
        oledDisplay->print("--");
    }

    // Line 3: Timer (if running or paused)
    if (stats.state == DryerState::RUNNING || stats.state == DryerState::PAUSED) {
        oledDisplay->setCursor(0, 24);

        // Elapsed time
        uint32_t elapsedMin = stats.elapsedTime / 60;
        uint32_t elapsedSec = stats.elapsedTime % 60;
        oledDisplay->print(String(elapsedMin));
        oledDisplay->print(":");
        if (elapsedSec < 10) oledDisplay->print("0");
        oledDisplay->print(String(elapsedSec));

        // Remaining time
        oledDisplay->print("/");
        uint32_t remainMin = stats.remainingTime / 60;
        oledDisplay->print(String(remainMin));
        oledDisplay->print("m");

        // PWM output
        oledDisplay->print(" ");
        oledDisplay->print(String((int)stats.pwmOutput));
    }

    oledDisplay->display();
}

/**
 * Initialize all hardware and create component instances
 */
void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    Serial.println("\n\n========================================");
    Serial.println("ESP32 Dryer Initializing...");
    Serial.println("========================================\n");

    // ==================== Create Sensor Components ====================
    Serial.println("Creating sensor components...");

    heaterSensor = new HeaterTempSensor(HEATER_TEMP_PIN);
    Serial.println("  - Heater temperature sensor created");

    boxSensor = new BoxTempHumiditySensor();
    Serial.println("  - Box temp/humidity sensor created");

    sensorManager = new SensorManager(heaterSensor, boxSensor);
    Serial.println("  - SensorManager created");

    // ==================== Create Display ====================
    Serial.println("\nCreating display...");

    oledDisplay = new OLEDDisplay(128, 64, 0x3C);
    Serial.println("  - OLED Display created");

    // ==================== Create Control Components ====================
    Serial.println("\nCreating control components...");

    heaterControl = new HeaterControl();
    Serial.println("  - HeaterControl created");

    pidController = new PIDController();
    Serial.println("  - PIDController created");

    safetyMonitor = new SafetyMonitor();
    Serial.println("  - SafetyMonitor created");

    // ==================== Create Placeholder Components ====================
    Serial.println("\nCreating placeholder components...");

    soundController = nullptr; // Will use nullptr for now
    Serial.println("  - Sound controller placeholder set");

    // ==================== Create Dryer Orchestrator ====================
    Serial.println("\nCreating Dryer orchestrator...");

    dryer = new Dryer(
        sensorManager,
        heaterControl,
        pidController,
        safetyMonitor,
        nullptr,  // settingsStorage - not implemented yet
        soundController
    );
    Serial.println("  - Dryer created");

    // ==================== Initialize All Components ====================
    Serial.println("\n========================================");
    Serial.println("Initializing components...");
    Serial.println("========================================\n");

    sensorManager->begin();
    Serial.println("  ✓ SensorManager initialized");

    oledDisplay->begin();
    Serial.println("  ✓ OLED Display initialized");

    heaterControl->begin();
    Serial.println("  ✓ HeaterControl initialized");

    pidController->begin();
    Serial.println("  ✓ PIDController initialized");

    safetyMonitor->begin();
    Serial.println("  ✓ SafetyMonitor initialized");

    // Initialize dryer (sets up callbacks, loads settings, etc.)
    dryer->begin();
    Serial.println("  ✓ Dryer initialized");

    // ==================== Show Startup Message ====================
    oledDisplay->clear();
    oledDisplay->setCursor(0, 0);
    oledDisplay->setTextSize(1);
    oledDisplay->println("Dryer Ready");
    oledDisplay->println("");
    oledDisplay->println("Starting in");
    oledDisplay->println("demo mode...");
    oledDisplay->display();
    delay(2000);

    // ==================== Configure and Start Demo ====================
    Serial.println("\n========================================");
    Serial.println("Starting Demo Mode");
    Serial.println("========================================\n");

    // Select PLA preset
    dryer->selectPreset(PresetType::PLA);
    Serial.println("Preset: PLA (50°C, 4 hours)");

    // Set NORMAL PID profile
    dryer->setPIDProfile(PIDProfile::NORMAL);
    Serial.println("PID Profile: NORMAL");

    // Start the dryer
    dryer->start();
    Serial.println("State: RUNNING");

    Serial.println("\n✓ System operational!");
    Serial.println("========================================\n");

    // Initial sensor status
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

    // ==================== Update All Components ====================

    // Update sensors (reads at configured intervals)
    sensorManager->update(currentMillis);

    // Update safety monitor (checks timeouts, limits)
    safetyMonitor->update(currentMillis);

    // Update dryer (state machine, PID, timing, persistence)
    dryer->update(currentMillis);

    // ==================== Update Display ====================

    if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        lastDisplayUpdate = currentMillis;
        updateDisplay();
    }

    // Small delay to prevent overwhelming the system
    delay(10);
}