/**
 * ESP32 Dryer - Main Application
 * MODIFIED FOR MANUAL PWM TESTING
 *
 * Integrates all control components with sensor display
 */

#include <Arduino.h>
#include "Config.h"
#include "Types.h"
#include <esp_task_wdt.h>  // Hardware watchdog timer

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

// Mock storage for now (until LittleFS implementation is complete)
#ifdef UNIT_TEST
    #include "test/mocks/MockSettingsStorage.h"
#else
    // Temporary mock for ESP32 until SettingsStorage is implemented
    class MockSettingsStorage : public ISettingsStorage {
    public:
        void begin() override {}
        void loadSettings() override {}
        void saveSettings() override {}
        void saveCustomPreset(const DryingPreset& preset) override {}
        DryingPreset loadCustomPreset() override { return DryingPreset(); }
        void saveSoundEnabled(bool enabled) override {}
        bool loadSoundEnabled() override { return true; }
        void saveRuntimeState(DryerState state, uint32_t elapsed,
                             float targetTemp, uint32_t targetTime,
                             PresetType preset, uint32_t timestamp) override {}
        bool hasValidRuntimeState() override { return false; }
        void loadRuntimeState() override {}
        void clearRuntimeState() override {}
        void saveEmergencyState(const String& reason) override {}
    };
#endif

// Watchdog configuration
constexpr uint32_t WATCHDOG_TIMEOUT_SECONDS = 10;  // Reset if loop doesn't run for 10 seconds

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

// Display update timing
uint32_t lastDisplayUpdate = 0;

// Serial command buffer
String serialCommand = "";
constexpr size_t MAX_SERIAL_COMMAND_LENGTH = 100;  // Prevent buffer overflow

// ==================== MANUAL PWM CONTROL MODE ====================
bool manualPWMMode = false;
uint8_t manualPWMValue = 0;
uint32_t manualModeStartTime = 0;

/**
 * Enhanced display showing both sensor data and PWM status
 * Display: 128x32 pixels
 * Line 1 (0-15px): Mode + Large Heater Temp
 * Line 2 (16-23px): PWM duty cycle
 * Line 3 (24-31px): Box data
 */
void updateDisplay() {
    oledDisplay->clear();

    // Get current sensor readings
    float heaterTemp = sensorManager->getHeaterTemp();
    bool heaterValid = sensorManager->isHeaterTempValid();
    float boxTemp = sensorManager->getBoxTemp();
    float boxHumidity = sensorManager->getBoxHumidity();
    bool boxValid = sensorManager->isBoxDataValid();

    // ==================== Line 1: Mode + Heater Temp (Large) ====================
    oledDisplay->setCursor(0, 0);
    oledDisplay->setTextSize(1);

    // Show mode (3 chars)
    if (manualPWMMode) {
        oledDisplay->print("MAN");
    } else {
        CurrentStats stats = dryer->getCurrentStats();
        switch(stats.state) {
            case DryerState::READY:
                oledDisplay->print("RDY");
                break;
            case DryerState::RUNNING:
                oledDisplay->print("RUN");
                break;
            case DryerState::PAUSED:
                oledDisplay->print("PAU");
                break;
            case DryerState::FINISHED:
                oledDisplay->print("FIN");
                break;
            case DryerState::FAILED:
                oledDisplay->print("ERR");
                break;
            case DryerState::POWER_RECOVERED:
                oledDisplay->print("PWR");
                break;
        }
    }

    // Heater temp - LARGE
    oledDisplay->setTextSize(2);
    oledDisplay->setCursor(24, 0);

    if (heaterValid) {
        oledDisplay->print(String(heaterTemp, 1));
        oledDisplay->setTextSize(1);
        oledDisplay->print("C");
    } else {
        oledDisplay->print("--C");
    }

    // ==================== Line 2: PWM Info ====================
    oledDisplay->setTextSize(1);
    oledDisplay->setCursor(0, 16);

    if (manualPWMMode) {
        oledDisplay->print("PWM: ");
        oledDisplay->print(String(manualPWMValue));
        oledDisplay->print("%");

        // Show elapsed time in manual mode
        uint32_t elapsedSec = (millis() - manualModeStartTime) / 1000;
        uint32_t mins = elapsedSec / 60;
        uint32_t secs = elapsedSec % 60;
        oledDisplay->print(" ");
        oledDisplay->print(String(mins));
        oledDisplay->print(":");
        if (secs < 10) oledDisplay->print("0");
        oledDisplay->print(String(secs));
    } else {
        CurrentStats stats = dryer->getCurrentStats();
        if (stats.state == DryerState::RUNNING || stats.state == DryerState::PAUSED) {
            // Show timer
            uint32_t elapsedMin = stats.elapsedTime / 60;
            uint32_t elapsedSec = stats.elapsedTime % 60;
            oledDisplay->print(String(elapsedMin));
            oledDisplay->print(":");
            if (elapsedSec < 10) oledDisplay->print("0");
            oledDisplay->print(String(elapsedSec));

            oledDisplay->print(" /");
            uint32_t remainMin = stats.remainingTime / 60;
            oledDisplay->print(String(remainMin));
            oledDisplay->print("m");

            // PWM
            oledDisplay->print(" P:");
            oledDisplay->print(String((int)stats.pwmOutput));
        } else {
            oledDisplay->print("Ready");
        }
    }

    // ==================== Line 3: Box data ====================
    oledDisplay->setCursor(0, 24);
    oledDisplay->setTextSize(1);
    if (boxValid) {
        oledDisplay->print("B:");
        oledDisplay->print(String(boxTemp, 1));
        oledDisplay->print("C ");
        oledDisplay->print(String(boxHumidity, 0));
        oledDisplay->print("%");
    } else {
        oledDisplay->print("Box: --");
    }

    oledDisplay->display();
}

/**
 * Handle serial commands for controlling the dryer
 * Commands:
 *   manual <0-100>    - Enter manual PWM mode with duty cycle 0-100%
 *   manual off        - Exit manual mode, return to auto
 *   start             - Start drying cycle (auto mode only)
 *   pause             - Pause current cycle (auto mode only)
 *   resume            - Resume from pause (auto mode only)
 *   stop              - Stop and return to ready (auto mode only)
 *   reset             - Reset to ready state
 *   preset pla        - Select PLA preset
 *   preset petg       - Select PETG preset
 *   preset custom     - Select custom preset
 *   pid soft          - Set PID profile to SOFT
 *   pid normal        - Set PID profile to NORMAL
 *   pid strong        - Set PID profile to STRONG
 *   sound on          - Enable sound
 *   sound off         - Disable sound
 *   status            - Print current status
 *   help              - Show available commands
 */
void handleSerialCommand(String cmd) {
    cmd.trim();
    cmd.toLowerCase();

    // ==================== MANUAL PWM COMMANDS ====================
    if (cmd.startsWith("manual ")) {
        String arg = cmd.substring(7);
        arg.trim();

        if (arg == "off") {
            // Exit manual mode
            manualPWMMode = false;
            heaterControl->stop(millis());
            Serial.println("✓ Manual PWM mode OFF");
            Serial.println("→ Returned to automatic control");
            return;
        }

        // Parse PWM value
        int pwm = arg.toInt();
        if (pwm < 0 || pwm > 100) {
            Serial.println("✗ PWM value must be 0-100");
            return;
        }

        // Enter/update manual mode
        if (!manualPWMMode) {
            manualPWMMode = true;
            manualModeStartTime = millis();
            heaterControl->start(millis());
            Serial.println("✓ Manual PWM mode ACTIVE");
            Serial.println("⚠ WARNING: PID control DISABLED");
            Serial.println("⚠ Monitor temperatures closely!");
        }

        manualPWMValue = pwm;
        heaterControl->setPWM(pwm);

        Serial.print("✓ PWM set to ");
        Serial.print(pwm);
        Serial.println("%");
        return;
    }

    // Check if in manual mode for auto commands
    if (manualPWMMode && (cmd == "start" || cmd == "pause" ||
                          cmd == "resume" || cmd == "stop")) {
        Serial.println("✗ Cannot use auto commands in manual PWM mode");
        Serial.println("→ Use 'manual off' to exit manual mode first");
        return;
    }

    // ==================== AUTOMATIC MODE COMMANDS ====================
    if (cmd == "start") {
        dryer->start();
        Serial.println("✓ Started");
    }
    else if (cmd == "pause") {
        dryer->pause();
        Serial.println("✓ Paused");
    }
    else if (cmd == "resume") {
        dryer->resume();
        Serial.println("✓ Resumed");
    }
    else if (cmd == "stop") {
        dryer->stop();
        Serial.println("✓ Stopped");
    }
    else if (cmd == "reset") {
        if (manualPWMMode) {
            manualPWMMode = false;
            heaterControl->stop(millis());
            Serial.println("✓ Manual mode OFF");
        }
        dryer->reset();
        Serial.println("✓ Reset");
    }
    else if (cmd == "preset pla") {
        dryer->selectPreset(PresetType::PLA);
        Serial.println("✓ PLA preset selected (50°C, 4h)");
    }
    else if (cmd == "preset petg") {
        dryer->selectPreset(PresetType::PETG);
        Serial.println("✓ PETG preset selected (65°C, 5h)");
    }
    else if (cmd == "preset custom") {
        dryer->selectPreset(PresetType::CUSTOM);
        Serial.println("✓ Custom preset selected");
    }
    else if (cmd == "pid soft") {
        dryer->setPIDProfile(PIDProfile::SOFT);
        Serial.println("✓ PID profile: SOFT");
    }
    else if (cmd == "pid normal") {
        dryer->setPIDProfile(PIDProfile::NORMAL);
        Serial.println("✓ PID profile: NORMAL");
    }
    else if (cmd == "pid strong") {
        dryer->setPIDProfile(PIDProfile::STRONG);
        Serial.println("✓ PID profile: STRONG");
    }
    else if (cmd == "sound on") {
        dryer->setSoundEnabled(true);
        Serial.println("✓ Sound enabled");
    }
    else if (cmd == "sound off") {
        dryer->setSoundEnabled(false);
        Serial.println("✓ Sound disabled");
    }
    else if (cmd == "status") {
        Serial.println("\n========== DRYER STATUS ==========");

        // Manual mode status
        Serial.print("Mode: ");
        if (manualPWMMode) {
            Serial.println("MANUAL PWM");
            Serial.print("  PWM Duty Cycle: ");
            Serial.print(manualPWMValue);
            Serial.println("%");

            uint32_t elapsedSec = (millis() - manualModeStartTime) / 1000;
            Serial.print("  Elapsed: ");
            Serial.print(elapsedSec / 60);
            Serial.print(":");
            if (elapsedSec % 60 < 10) Serial.print("0");
            Serial.println(elapsedSec % 60);
        } else {
            Serial.println("AUTOMATIC");

            CurrentStats stats = dryer->getCurrentStats();

            // State
            Serial.print("  State: ");
            switch(stats.state) {
                case DryerState::READY:
                    Serial.println("READY");
                    break;
                case DryerState::RUNNING:
                    Serial.println("RUNNING");
                    break;
                case DryerState::PAUSED:
                    Serial.println("PAUSED");
                    break;
                case DryerState::FINISHED:
                    Serial.println("FINISHED");
                    break;
                case DryerState::FAILED:
                    Serial.println("FAILED");
                    break;
                case DryerState::POWER_RECOVERED:
                    Serial.println("POWER_RECOVERED");
                    break;
            }

            // Active preset
            Serial.print("  Preset: ");
            switch(stats.activePreset) {
                case PresetType::PLA:
                    Serial.println("PLA");
                    break;
                case PresetType::PETG:
                    Serial.println("PETG");
                    break;
                case PresetType::CUSTOM:
                    Serial.println("CUSTOM");
                    break;
            }

            // PID profile
            Serial.print("  PID Profile: ");
            switch(dryer->getPIDProfile()) {
                case PIDProfile::SOFT:
                    Serial.println("SOFT");
                    break;
                case PIDProfile::NORMAL:
                    Serial.println("NORMAL");
                    break;
                case PIDProfile::STRONG:
                    Serial.println("STRONG");
                    break;
            }
        }

        // Temperatures (same for both modes)
        Serial.print("\nHeater Temp: ");
        if (sensorManager->isHeaterTempValid()) {
            Serial.print(sensorManager->getHeaterTemp(), 1);
            Serial.println("°C");
        } else {
            Serial.println("INVALID");
        }

        Serial.print("Box Temp: ");
        if (sensorManager->isBoxDataValid()) {
            Serial.print(sensorManager->getBoxTemp(), 1);
            Serial.println("°C");
        } else {
            Serial.println("INVALID");
        }

        Serial.print("Box Humidity: ");
        if (sensorManager->isBoxDataValid()) {
            Serial.print(sensorManager->getBoxHumidity(), 1);
            Serial.println("%");
        } else {
            Serial.println("INVALID");
        }

        if (!manualPWMMode) {
            CurrentStats stats = dryer->getCurrentStats();

            // Timer info
            if (stats.state == DryerState::RUNNING || stats.state == DryerState::PAUSED) {
                Serial.print("\nElapsed: ");
                Serial.print(stats.elapsedTime / 60);
                Serial.print(":");
                if (stats.elapsedTime % 60 < 10) Serial.print("0");
                Serial.println(stats.elapsedTime % 60);

                Serial.print("Remaining: ");
                Serial.print(stats.remainingTime / 60);
                Serial.println(" min");
            }

            // PWM output
            Serial.print("\nPWM Output: ");
            Serial.print((int)stats.pwmOutput);
            Serial.println(" / 255");

            // Sound
            Serial.print("Sound: ");
            Serial.println(dryer->isSoundEnabled() ? "ON" : "OFF");
        }

        Serial.println("==================================\n");
    }
    else if (cmd == "help" || cmd == "?") {
        Serial.println("\n========== AVAILABLE COMMANDS ==========");
        Serial.println("Manual PWM Control:");
        Serial.println("  manual <0-100>  - Set PWM duty cycle (bypasses PID)");
        Serial.println("  manual off      - Exit manual mode");
        Serial.println("\nState Control (Auto Mode):");
        Serial.println("  start           - Start drying cycle");
        Serial.println("  pause           - Pause current cycle");
        Serial.println("  resume          - Resume from pause");
        Serial.println("  stop            - Stop and return to ready");
        Serial.println("  reset           - Reset to ready state");
        Serial.println("\nPreset Selection:");
        Serial.println("  preset pla      - Select PLA preset (50°C, 4h)");
        Serial.println("  preset petg     - Select PETG preset (65°C, 5h)");
        Serial.println("  preset custom   - Select custom preset");
        Serial.println("\nPID Profile:");
        Serial.println("  pid soft        - Gentle heating (Kp=2.0)");
        Serial.println("  pid normal      - Balanced (Kp=4.0)");
        Serial.println("  pid strong      - Aggressive (Kp=6.0)");
        Serial.println("\nSettings:");
        Serial.println("  sound on        - Enable sound");
        Serial.println("  sound off       - Disable sound");
        Serial.println("\nInfo:");
        Serial.println("  status          - Print current status");
        Serial.println("  help            - Show this help");
        Serial.println("========================================\n");
    }
    else if (cmd.length() > 0) {
        Serial.print("✗ Unknown command: '");
        Serial.print(cmd);
        Serial.println("'");
        Serial.println("Type 'help' for available commands");
    }
}

/**
 * Process serial input with buffer overflow protection
 */
void processSerialInput() {
    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (serialCommand.length() > 0) {
                handleSerialCommand(serialCommand);
                serialCommand = "";
            }
        } else {
            // Prevent buffer overflow
            if (serialCommand.length() < MAX_SERIAL_COMMAND_LENGTH) {
                serialCommand += c;
            } else {
                // Buffer full - discard command and reset
                Serial.println("✗ Command too long (max 100 chars)");
                serialCommand = "";
            }
        }
    }
}

/**
 * Initialize hardware watchdog timer
 */
void setupWatchdog() {
    Serial.println("Configuring hardware watchdog timer...");

    esp_err_t result = esp_task_wdt_init(WATCHDOG_TIMEOUT_SECONDS, true);

    if (result == ESP_OK) {
        result = esp_task_wdt_add(NULL);

        if (result == ESP_OK) {
            Serial.print("  ✓ Hardware watchdog enabled (");
            Serial.print(WATCHDOG_TIMEOUT_SECONDS);
            Serial.println(" second timeout)");
        } else {
            Serial.println("  ✗ WARNING: Failed to add task to watchdog!");
        }
    } else {
        Serial.println("  ✗ WARNING: Failed to initialize watchdog!");
    }
}

/**
 * Initialize all hardware and create component instances
 */
void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    delay(100);

    Serial.println("\n\n========================================");
    Serial.println("ESP32 Dryer - MANUAL PWM TEST MODE");
    Serial.println("========================================\n");

    setupWatchdog();
    Serial.println();

    // Create components
    Serial.println("Creating components...");

    heaterSensor = new HeaterTempSensor(HEATER_TEMP_PIN);
    boxSensor = new BoxTempHumiditySensor();
    sensorManager = new SensorManager(heaterSensor, boxSensor);
    oledDisplay = new OLEDDisplay(128, 32, 0x3C);
    heaterControl = new HeaterControl();
    pidController = new PIDController();
    safetyMonitor = new SafetyMonitor();
    settingsStorage = new MockSettingsStorage();
    soundController = nullptr;

    dryer = new Dryer(
        sensorManager,
        heaterControl,
        pidController,
        safetyMonitor,
        settingsStorage,
        soundController
    );

    Serial.println("  ✓ All components created");

    // Initialize components
    Serial.println("\nInitializing components...");

    sensorManager->begin();
    oledDisplay->begin();
    heaterControl->begin(millis());
    pidController->begin();
    safetyMonitor->begin();
    settingsStorage->begin();
    dryer->begin(millis());

    Serial.println("  ✓ All components initialized");

    // Show startup message
    oledDisplay->clear();
    oledDisplay->setCursor(0, 0);
    oledDisplay->setTextSize(1);
    oledDisplay->println("Manual PWM");
    oledDisplay->println("Test Mode");
    oledDisplay->println("");
    oledDisplay->println("Ready...");
    oledDisplay->display();
    delay(2000);

    Serial.println("\n========================================");
    Serial.println("✓ System Ready for Manual PWM Testing");
    Serial.println("========================================\n");
    Serial.println("⚠ WARNING: PID control will be BYPASSED");
    Serial.println("⚠ Monitor temperatures continuously!");
    Serial.println("\nUsage:");
    Serial.println("  manual 30    → Set 30% duty cycle");
    Serial.println("  manual 50    → Set 50% duty cycle");
    Serial.println("  manual 0     → Turn heater OFF");
    Serial.println("  manual off   → Exit manual mode");
    Serial.println("  status       → Show current readings");
    Serial.println("\nType 'help' for all commands\n");
}

void loop() {
    uint32_t currentMillis = millis();

    // Pet the watchdog
    esp_task_wdt_reset();

    // Process serial commands
    processSerialInput();

    // Update sensors
    sensorManager->update(currentMillis);

    // Update safety monitor (even in manual mode!)
    safetyMonitor->update(currentMillis);

    // Update heater control (software PWM timing)
    heaterControl->update(currentMillis);

    // Update dryer ONLY if not in manual mode
    if (!manualPWMMode) {
        dryer->update(currentMillis);
    }

    // Update display
    if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        lastDisplayUpdate = currentMillis;
        updateDisplay();
    }

    delay(10);
}