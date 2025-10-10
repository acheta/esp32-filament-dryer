/**
 * ESP32 Dryer - Main Application
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
#include "interfaces/IButtonManager.h"
#include "interfaces/IMenuController.h"



// Implementations
#include "sensors/HeaterTempSensor.h"
#include "sensors/BoxTempHumiditySensor.h"
#include "sensors/SensorManager.h"
#include "control/HeaterControl.h"
#include "control/PIDController.h"
#include "control/SafetyMonitor.h"
#include "userInterface/OLEDDisplay.h"
#include "userInterface/ButtonManager.h"
#include "userInterface/MenuController.h"
#include "userInterface/UIController.h"

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
IButtonManager* buttonManager = nullptr;
IMenuController* menuController = nullptr;
UIController* uiController = nullptr;


// Serial command buffer
String serialCommand = "";
constexpr size_t MAX_SERIAL_COMMAND_LENGTH = 100;  // Prevent buffer overflow



/**
 * Handle serial commands for controlling the dryer
 * Commands:
 *   start         - Start drying cycle
 *   pause         - Pause current cycle
 *   resume        - Resume from pause
 *   stop          - Stop and return to ready
 *   reset         - Reset to ready state
 *   preset pla    - Select PLA preset
 *   preset petg   - Select PETG preset
 *   preset custom - Select custom preset
 *   pid soft      - Set PID profile to SOFT
 *   pid normal    - Set PID profile to NORMAL
 *   pid strong    - Set PID profile to STRONG
 *   sound on      - Enable sound
 *   sound off     - Disable sound
 *   status        - Print current status
 *   help          - Show available commands
 */
void handleSerialCommand(String cmd) {
    cmd.trim();
    cmd.toLowerCase();

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
        CurrentStats stats = dryer->getCurrentStats();

        Serial.println("\n========== DRYER STATUS ==========");

        // State
        Serial.print("State: ");
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
        Serial.print("Preset: ");
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
        Serial.print("PID Profile: ");
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

        // Temperatures
        Serial.print("Heater Temp: ");
        if (sensorManager->isHeaterTempValid()) {
            Serial.print(stats.currentTemp, 1);
            Serial.print("°C / ");
            Serial.print(stats.targetTemp, 0);
            Serial.println("°C");
        } else {
            Serial.println("INVALID");
        }

        Serial.print("Box Temp: ");
        if (sensorManager->isBoxDataValid()) {
            Serial.print(stats.boxTemp, 1);
            Serial.println("°C");
        } else {
            Serial.println("INVALID");
        }

        Serial.print("Box Humidity: ");
        if (sensorManager->isBoxDataValid()) {
            Serial.print(stats.boxHumidity, 1);
            Serial.println("%");
        } else {
            Serial.println("INVALID");
        }

        // Timer info
        if (stats.state == DryerState::RUNNING || stats.state == DryerState::PAUSED) {
            Serial.print("Elapsed: ");
            Serial.print(stats.elapsedTime / 60);
            Serial.print(":");
            if (stats.elapsedTime % 60 < 10) Serial.print("0");
            Serial.println(stats.elapsedTime % 60);

            Serial.print("Remaining: ");
            Serial.print(stats.remainingTime / 60);
            Serial.println(" min");
        }

        // PWM output
        Serial.print("PWM Output: ");
        Serial.print((int)stats.pwmOutput);
        Serial.println(" / 255");

        // Sound
        Serial.print("Sound: ");
        Serial.println(dryer->isSoundEnabled() ? "ON" : "OFF");

        Serial.println("==================================\n");
    }
    else if (cmd == "help" || cmd == "?") {
        Serial.println("\n========== AVAILABLE COMMANDS ==========");
        Serial.println("State Control:");
        Serial.println("  start         - Start drying cycle");
        Serial.println("  pause         - Pause current cycle");
        Serial.println("  resume        - Resume from pause");
        Serial.println("  stop          - Stop and return to ready");
        Serial.println("  reset         - Reset to ready state");
        Serial.println("\nPreset Selection:");
        Serial.println("  preset pla    - Select PLA preset (50°C, 4h)");
        Serial.println("  preset petg   - Select PETG preset (65°C, 5h)");
        Serial.println("  preset custom - Select custom preset");
        Serial.println("\nPID Profile:");
        Serial.println("  pid soft      - Gentle heating (Kp=2.0)");
        Serial.println("  pid normal    - Balanced (Kp=4.0)");
        Serial.println("  pid strong    - Aggressive (Kp=6.0)");
        Serial.println("\nSettings:");
        Serial.println("  sound on      - Enable sound");
        Serial.println("  sound off     - Disable sound");
        Serial.println("\nInfo:");
        Serial.println("  status        - Print current status");
        Serial.println("  help          - Show this help");
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
 *
 * Protects against software hangs (infinite loops, deadlocks, stack overflow).
 * If loop() doesn't call esp_task_wdt_reset() within timeout, ESP32 resets itself.
 *
 * NOTE: This does NOT protect against firmware corruption or hardware failures.
 *       For complete safety, consider adding external 555 timer watchdog circuit.
 */
void setupWatchdog() {
    Serial.println("Configuring hardware watchdog timer...");

    // Initialize watchdog with timeout (older API compatible with more ESP-IDF versions)
    esp_err_t result = esp_task_wdt_init(WATCHDOG_TIMEOUT_SECONDS, true);
    // Parameters: timeout in seconds, panic on timeout (true = reset ESP32)

    if (result == ESP_OK) {
        // Add current task (loop task) to watchdog monitoring
        result = esp_task_wdt_add(NULL);  // NULL = current task

        if (result == ESP_OK) {
            Serial.print("  ✓ Hardware watchdog enabled (");
            Serial.print(WATCHDOG_TIMEOUT_SECONDS);
            Serial.println(" second timeout)");
            Serial.println("  → System will auto-reset if loop() hangs");
        } else {
            Serial.println("  ✗ WARNING: Failed to add task to watchdog!");
            Serial.println("  → Watchdog protection NOT active");
        }
    } else {
        Serial.println("  ✗ WARNING: Failed to initialize watchdog!");
        Serial.println("  → Watchdog protection NOT active");
    }
}

/**
 * Initialize all hardware and create component instances
 */
void setup() {// Initialize serial for debugging
    Serial.begin(115200);
    delay(100);

    Serial.println("\n\n========================================");
    Serial.println("ESP32 Dryer Initializing...");
    Serial.println("========================================\n");

    // ... [existing watchdog setup] ...

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

    oledDisplay = new OLEDDisplay(DISPLAY_WIDTH, DISPLAY_HEIGHT, 0x3C);
    Serial.println("  - OLED Display created");

    // ==================== Create Control Components ====================
    Serial.println("\nCreating control components...");
    heaterControl = new HeaterControl();
    Serial.println("  - HeaterControl created");

    pidController = new PIDController();
    Serial.println("  - PIDController created");

    safetyMonitor = new SafetyMonitor();
    Serial.println("  - SafetyMonitor created");

    // ==================== Create Storage & Sound ====================
    Serial.println("\nCreating storage and sound components...");
    settingsStorage = new MockSettingsStorage();
    Serial.println("  - SettingsStorage (mock) created");

    soundController = nullptr; // Not implemented yet
    Serial.println("  - Sound controller placeholder set");

    // ==================== Create Dryer ====================
    Serial.println("\nCreating Dryer orchestrator...");
    dryer = new Dryer(
        sensorManager,
        heaterControl,
        pidController,
        safetyMonitor,
        settingsStorage,
        soundController
    );
    Serial.println("  - Dryer created");

    // ==================== Create UI Components ====================
    Serial.println("\nCreating UI components...");

    // Create ButtonManager first
    buttonManager = new ButtonManager();
    Serial.println("  - ButtonManager created");

    // Create MenuController second
    menuController = new MenuController();
    Serial.println("  - MenuController created");

    // Create UIController last (needs all other components)
    uiController = new UIController(
        oledDisplay,      // Must not be null
        menuController,   // Must not be null
        buttonManager,    // Must not be null
        soundController,  // Can be null
        dryer             // Must not be null
    );
    Serial.println("  - UIController created");

    // ==================== Initialize All Components ====================
    Serial.println("\n========================================");
    Serial.println("Initializing components...");
    Serial.println("========================================\n");

    // Initialize basic components first
    sensorManager->begin();
    Serial.println("  ✓ SensorManager initialized");

    oledDisplay->begin();
    Serial.println("  ✓ OLED Display initialized");

    heaterControl->begin(millis());
    Serial.println("  ✓ HeaterControl initialized");

    pidController->begin();
    Serial.println("  ✓ PIDController initialized");

    safetyMonitor->begin();
    Serial.println("  ✓ SafetyMonitor initialized");

    settingsStorage->begin();
    Serial.println("  ✓ SettingsStorage initialized");

    // Initialize dryer (sets up callbacks, loads settings)
    dryer->begin(millis());
    Serial.println("  ✓ Dryer initialized");

    // Initialize UI components AFTER dryer
    Serial.println("\nInitializing UI components...");

    buttonManager->begin();
    Serial.println("  ✓ ButtonManager initialized");

    // UIController.begin() will set up all callbacks
    Serial.println("\nInitializing UIController...");
    uiController->begin();
    Serial.println("  ✓ UIController initialized");

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

    dryer->selectPreset(PresetType::PLA);
    Serial.println("Preset: PLA (50°C, 4 hours)");

    dryer->setPIDProfile(PIDProfile::STRONG);
    Serial.println("PID Profile: STRONG");

    dryer->start();
    Serial.println("State: RUNNING");

    Serial.println("\n✓ System operational!");
    Serial.println("========================================\n");
    Serial.println("Type 'help' for available commands\n");
}

void loop() {
    uint32_t currentMillis = millis();

    // ==================== Pet the Watchdog ====================
    // CRITICAL: This must be called every loop iteration
    // If loop hangs for >10 seconds, watchdog triggers ESP32 reset
    esp_task_wdt_reset();

    // ==================== Process Serial Commands ====================
    processSerialInput();

    // ==================== Update All Components ====================

    // Update sensors (reads at configured intervals)
    sensorManager->update(currentMillis);
    safetyMonitor->update(currentMillis);
    dryer->update(currentMillis);
    heaterControl->update(currentMillis);

    // Update UI (replaces old display update code)
    uiController->update(currentMillis);

    delay(10);
}