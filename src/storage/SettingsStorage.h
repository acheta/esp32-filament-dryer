
#ifndef SETTINGS_STORAGE_H
#define SETTINGS_STORAGE_H

#include "../interfaces/ISettingsStorage.h"
#include "../Config.h"

#ifndef UNIT_TEST
    #include <LittleFS.h>
    #include <ArduinoJson.h>
#else
    #include "../../test/mocks/arduino_mock.h"
    // MockFileSystem.h and MockArduinoJson.h are included via arduino_mock.h
#endif

/**
 * SettingsStorage - LittleFS-based persistent storage
 *
 * Features:
 * - Versioned settings for future migration
 * - Separate files for settings and runtime state
 * - Corruption detection and recovery
 * - Graceful degradation on write failures
 * - Immediate saves on setting changes
 *
 * File Structure:
 * - /settings.json: User preferences (preset, PID, sound, custom preset)
 * - /runtime.json: Current cycle state for power recovery
 */
class SettingsStorage : public ISettingsStorage {
private:
    // Versioning for future migrations
    static constexpr uint8_t SETTINGS_VERSION = 1;
    static constexpr uint8_t RUNTIME_VERSION = 1;

    // Storage state
    bool initialized;
    bool storageHealthy;  // False if critical errors detected
    String lastError;

    // Cached settings
    DryingPreset customPreset;
    PresetType selectedPreset;
    PIDProfile selectedPIDProfile;
    bool soundEnabled;

    // Cached runtime state
    bool hasValidRuntime;
    DryerState runtimeState;
    uint32_t runtimeElapsed;
    float runtimeTargetTemp;
    uint32_t runtimeTargetTime;
    PresetType runtimePreset;
    uint32_t runtimeTimestamp;

    /**
     * Initialize LittleFS filesystem
     * Formats if mount fails
     */
    bool initializeFilesystem() {
#ifndef UNIT_TEST
        Serial.println("Initializing LittleFS...");
#endif

        if (!LittleFS.begin(true)) {  // true = format on fail
            lastError = "LittleFS mount failed";
#ifndef UNIT_TEST
            Serial.println("  ✗ LittleFS mount failed");
#endif
            return false;
        }

#ifndef UNIT_TEST
        Serial.println("  ✓ LittleFS mounted successfully");

        // Check filesystem health
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();

        Serial.print("  Storage: ");
        Serial.print(usedBytes);
        Serial.print(" / ");
        Serial.print(totalBytes);
        Serial.println(" bytes used");
#endif

        return true;
    }

    /**
     * Verify a JSON file is readable
     * Returns true if file exists and can be parsed
     */
    bool verifyJsonFile(const char* path) {
        if (!LittleFS.exists(path)) {
            return false;
        }

        File file = LittleFS.open(path, "r");
        if (!file) {
            return false;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        return (error == DeserializationError::Ok);
    }

    /**
     * Format entire filesystem and recreate files
     * Nuclear option for corruption recovery
     */
    void formatAndRecreate() {
#ifndef UNIT_TEST
        Serial.println("Formatting LittleFS...");
#endif

        LittleFS.format();

        if (!LittleFS.begin(true)) {
            lastError = "Failed to reinitialize after format";
            storageHealthy = false;
            return;
        }

#ifndef UNIT_TEST
        Serial.println("  ✓ Filesystem formatted");
#endif

        // Create fresh settings with defaults
        saveSettingsInternal();

        // Verify we can read it back
        if (!verifyJsonFile(SETTINGS_FILE)) {
            lastError = "Settings file unreadable after creation";
            storageHealthy = false;
#ifndef UNIT_TEST
            Serial.println("  ✗ CRITICAL: Cannot create valid settings file");
#endif
        } else {
#ifndef UNIT_TEST
            Serial.println("  ✓ Settings file created and verified");
#endif
        }
    }

    /**
     * Load settings from JSON file
     */
    bool loadSettingsInternal() {
        if (!LittleFS.exists(SETTINGS_FILE)) {
#ifndef UNIT_TEST
            Serial.println("Settings file not found - using defaults");
#endif
            return false;
        }

        File file = LittleFS.open(SETTINGS_FILE, "r");
        if (!file) {
            lastError = "Cannot open settings file";
            return false;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            lastError = String("Settings JSON parse error: ") + String(error.c_str());
#ifndef UNIT_TEST
            Serial.print("  ✗ ");
            Serial.println(lastError);
#endif
            return false;
        }

        // Version check (for future migrations)
        uint8_t version = doc["version"] | 0;
        if (version == 0 || version > SETTINGS_VERSION) {
            lastError = String("Unknown settings version: ") + String(version);
            return false;
        }

        // Load custom preset
        if (doc["customPreset"].is<JsonObject>()) {
            JsonObject preset = doc["customPreset"];
            customPreset.targetTemp = preset["temp"] | PRESET_CUSTOM_TEMP;
            customPreset.targetTime = preset["time"] | PRESET_CUSTOM_TIME;
            customPreset.maxOvershoot = preset["overshoot"] | PRESET_CUSTOM_OVERSHOOT;
        }

        // Load selected preset
        String presetStr = doc["selectedPreset"] | "PLA";
        if (presetStr == "PLA") selectedPreset = PresetType::PLA;
        else if (presetStr == "PETG") selectedPreset = PresetType::PETG;
        else if (presetStr == "CUSTOM") selectedPreset = PresetType::CUSTOM;
        else selectedPreset = PresetType::PLA;

        // Load PID profile
        String pidStr = doc["pidProfile"] | "NORMAL";
        if (pidStr == "SOFT") selectedPIDProfile = PIDProfile::SOFT;
        else if (pidStr == "NORMAL") selectedPIDProfile = PIDProfile::NORMAL;
        else if (pidStr == "STRONG") selectedPIDProfile = PIDProfile::STRONG;
        else selectedPIDProfile = PIDProfile::NORMAL;

        // Load sound setting
        soundEnabled = doc["soundEnabled"] | true;

#ifndef UNIT_TEST
        Serial.println("  ✓ Settings loaded");
#endif
        return true;
    }

    /**
     * Save settings to JSON file
     */
    bool saveSettingsInternal() {
        JsonDocument doc;

        doc["version"] = SETTINGS_VERSION;

        // Custom preset
        JsonObject preset = doc["customPreset"].to<JsonObject>();
        preset["temp"] = customPreset.targetTemp;
        preset["time"] = customPreset.targetTime;
        preset["overshoot"] = customPreset.maxOvershoot;

        // Selected preset
        switch (selectedPreset) {
            case PresetType::PLA: doc["selectedPreset"] = "PLA"; break;
            case PresetType::PETG: doc["selectedPreset"] = "PETG"; break;
            case PresetType::CUSTOM: doc["selectedPreset"] = "CUSTOM"; break;
        }

        // PID profile
        switch (selectedPIDProfile) {
            case PIDProfile::SOFT: doc["pidProfile"] = "SOFT"; break;
            case PIDProfile::NORMAL: doc["pidProfile"] = "NORMAL"; break;
            case PIDProfile::STRONG: doc["pidProfile"] = "STRONG"; break;
        }

        // Sound setting
        doc["soundEnabled"] = soundEnabled;

        // Write to file
        File file = LittleFS.open(SETTINGS_FILE, "w");
        if (!file) {
            lastError = "Cannot open settings file for writing";
#ifndef UNIT_TEST
            Serial.print("  ✗ ");
            Serial.println(lastError);
#endif
            return false;
        }

        size_t bytesWritten = serializeJson(doc, file);
        file.close();

        if (bytesWritten == 0) {
            lastError = "Failed to write settings";
            return false;
        }

        return true;
    }

    /**
     * Load runtime state from JSON file
     * Loads whatever state is stored - no business logic filtering
     * Dryer layer decides which states are valid for recovery
     */
    bool loadRuntimeInternal() {
        if (!LittleFS.exists(RUNTIME_FILE)) {
            hasValidRuntime = false;
            return false;
        }

        File file = LittleFS.open(RUNTIME_FILE, "r");
        if (!file) {
            hasValidRuntime = false;
            return false;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            lastError = String("Runtime JSON parse error: ") + String(error.c_str());
            hasValidRuntime = false;
            return false;
        }

        // Version check
        uint8_t version = doc["version"] | 0;
        if (version == 0 || version > RUNTIME_VERSION) {
            hasValidRuntime = false;
            return false;
        }

        // Load state string and convert to enum
        // No filtering - load whatever was saved
        String stateStr = doc["state"] | "READY";
        if (stateStr == "RUNNING") {
            runtimeState = DryerState::RUNNING;
        } else if (stateStr == "PAUSED") {
            runtimeState = DryerState::PAUSED;
        } else if (stateStr == "FINISHED") {
            runtimeState = DryerState::FINISHED;
        } else if (stateStr == "FAILED") {
            runtimeState = DryerState::FAILED;
        } else {
            runtimeState = DryerState::READY;
        }

        runtimeElapsed = doc["elapsed"] | 0;
        runtimeTargetTemp = doc["targetTemp"] | 50.0;
        runtimeTargetTime = doc["targetTime"] | 14400;

        String presetStr = doc["preset"] | "PLA";
        if (presetStr == "PLA") runtimePreset = PresetType::PLA;
        else if (presetStr == "PETG") runtimePreset = PresetType::PETG;
        else if (presetStr == "CUSTOM") runtimePreset = PresetType::CUSTOM;
        else runtimePreset = PresetType::PLA;

        runtimeTimestamp = doc["timestamp"] | 0;

#ifndef UNIT_TEST
        // Log the timestamp for informational purposes
        Serial.print("  Runtime saved at timestamp: ");
        Serial.println(runtimeTimestamp);

        hasValidRuntime = true;
        Serial.print("  ✓ Runtime state loaded (");
        Serial.print(stateStr);
        Serial.println(")");
#else
        hasValidRuntime = true;
#endif
        return true;
    }

    /**
     * Save runtime state to JSON file
     */
    bool saveRuntimeInternal(DryerState state, uint32_t elapsed,
                            float targetTemp, uint32_t targetTime,
                            PresetType preset, uint32_t timestamp) {
        JsonDocument doc;

        doc["version"] = RUNTIME_VERSION;

        // State
        switch (state) {
            case DryerState::RUNNING: doc["state"] = "RUNNING"; break;
            case DryerState::PAUSED: doc["state"] = "PAUSED"; break;
            default: doc["state"] = "READY"; break;
        }

        doc["elapsed"] = elapsed;
        doc["targetTemp"] = targetTemp;
        doc["targetTime"] = targetTime;

        // Preset
        switch (preset) {
            case PresetType::PLA: doc["preset"] = "PLA"; break;
            case PresetType::PETG: doc["preset"] = "PETG"; break;
            case PresetType::CUSTOM: doc["preset"] = "CUSTOM"; break;
        }

        doc["timestamp"] = timestamp;

        // Write to file
        File file = LittleFS.open(RUNTIME_FILE, "w");
        if (!file) {
            lastError = "Cannot open runtime file for writing";
            // Don't print error - this happens frequently during normal operation
            return false;
        }

        size_t bytesWritten = serializeJson(doc, file);
        file.close();

        return (bytesWritten > 0);
    }

public:
    SettingsStorage()
        : initialized(false),
          storageHealthy(true),
          selectedPreset(PresetType::PLA),
          selectedPIDProfile(PIDProfile::NORMAL),
          soundEnabled(true),
          hasValidRuntime(false),
          runtimeState(DryerState::READY),
          runtimeElapsed(0),
          runtimeTargetTemp(50.0),
          runtimeTargetTime(14400),
          runtimePreset(PresetType::PLA),
          runtimeTimestamp(0) {

        // Initialize custom preset with defaults
        customPreset.targetTemp = PRESET_CUSTOM_TEMP;
        customPreset.targetTime = PRESET_CUSTOM_TIME;
        customPreset.maxOvershoot = PRESET_CUSTOM_OVERSHOOT;
    }

    void begin() override {
#ifndef UNIT_TEST
        Serial.println("\n========================================");
        Serial.println("Initializing SettingsStorage");
        Serial.println("========================================");
#endif

        // Initialize filesystem
        if (!initializeFilesystem()) {
            storageHealthy = false;
            initialized = true;
#ifndef UNIT_TEST
            Serial.println("✗ Storage initialization failed");
            Serial.println("  System will continue with defaults");
            Serial.println("========================================\n");
#endif
            return;
        }

#ifndef UNIT_TEST
        // Check for corrupted files
        bool settingsValid = verifyJsonFile(SETTINGS_FILE);
        bool runtimeValid = verifyJsonFile(RUNTIME_FILE);

        if (!settingsValid && LittleFS.exists(SETTINGS_FILE)) {
            Serial.println("⚠ Corrupted settings file detected");
            formatAndRecreate();
        } else if (!settingsValid) {
            // File doesn't exist - create it
            Serial.println("Creating initial settings file...");
            saveSettingsInternal();

            if (!verifyJsonFile(SETTINGS_FILE)) {
                lastError = "Cannot create valid settings file";
                storageHealthy = false;
                Serial.println("✗ CRITICAL: Settings file verification failed");
            }
        }

        if (!runtimeValid && LittleFS.exists(RUNTIME_FILE)) {
            Serial.println("⚠ Corrupted runtime file detected - removing");
            LittleFS.remove(RUNTIME_FILE);
        }
#endif

        // Load settings
        if (storageHealthy) {
            loadSettingsInternal();
        }

        // Load runtime state (for power recovery)
        loadRuntimeInternal();

        initialized = true;

#ifndef UNIT_TEST
        if (storageHealthy) {
            Serial.println("✓ Storage initialized successfully");
        } else {
            Serial.println("⚠ Storage initialized with errors");
            Serial.print("  Last error: ");
            Serial.println(lastError);
        }
        Serial.println("========================================\n");
#endif
    }

    void loadSettings() override {
        if (!initialized || !storageHealthy) {
            return;
        }
        loadSettingsInternal();
    }

    void saveSettings() override {
        if (!initialized) {
            return;
        }

        if (!saveSettingsInternal()) {
#ifndef UNIT_TEST
            Serial.println("⚠ Failed to save settings (system will continue)");
#endif
        }
    }

    void saveCustomPreset(const DryingPreset& preset) override {
        customPreset = preset;
        saveSettings();  // Save immediately
    }

    DryingPreset loadCustomPreset() override {
        return customPreset;
    }

    void saveSelectedPreset(PresetType preset) override {
        selectedPreset = preset;
        saveSettings();  // Save immediately
    }

    PresetType loadSelectedPreset() override {
        return selectedPreset;
    }

    void savePIDProfile(PIDProfile profile) override {
        selectedPIDProfile = profile;
        saveSettings();  // Save immediately
    }

    PIDProfile loadPIDProfile() override {
        return selectedPIDProfile;
    }

    void saveSoundEnabled(bool enabled) override {
        soundEnabled = enabled;
        saveSettings();  // Save immediately
    }

    bool loadSoundEnabled() override {
        return soundEnabled;
    }

    void saveRuntimeState(DryerState state, uint32_t elapsed,
                         float targetTemp, uint32_t targetTime,
                         PresetType preset, uint32_t timestamp) override {
        if (!initialized) {
            return;
        }

        // Cache the values
        runtimeState = state;
        runtimeElapsed = elapsed;
        runtimeTargetTemp = targetTemp;
        runtimeTargetTime = targetTime;
        runtimePreset = preset;
        runtimeTimestamp = timestamp;

        // Mark as valid in memory (cached)
        // Note: On restart, the state will be loaded and validated by Dryer
        hasValidRuntime = true;

        // Save to file (don't fail if write fails - graceful degradation)
        saveRuntimeInternal(state, elapsed, targetTemp, targetTime, preset, timestamp);
    }

    bool hasValidRuntimeState() override {
        return hasValidRuntime;
    }

    void loadRuntimeState() override {
        // Already loaded in begin()
        // This is a no-op but required by interface
    }

    void clearRuntimeState() override {
        hasValidRuntime = false;

        if (initialized && LittleFS.exists(RUNTIME_FILE)) {
            LittleFS.remove(RUNTIME_FILE);
        }
    }

    void saveEmergencyState(const String& reason) override {
        if (!initialized) {
            return;
        }

#ifndef UNIT_TEST
        // Save special emergency marker
        File file = LittleFS.open("/emergency.txt", "w");
        if (file) {
            file.print(reason);
            file.close();
        }
#endif

        // Also save runtime state marked as FAILED
        saveRuntimeInternal(DryerState::FAILED, runtimeElapsed,
                          runtimeTargetTemp, runtimeTargetTime,
                          runtimePreset, millis());
    }

    // ==================== Additional Methods ====================

    bool isHealthy() const {
        return storageHealthy;
    }

    String getLastError() const {
        return lastError;
    }

    String getInitErrorMessage() const {
        if (storageHealthy) {
            return "";
        }
        return lastError.length() > 0 ? lastError : "Storage initialization failed";
    }

    bool isInitialized() const {
        return initialized;
    }

    // Expose cached runtime values for Dryer to use during recovery
    DryerState getRuntimeState() const override { return runtimeState; }
    uint32_t getRuntimeElapsed() const override { return runtimeElapsed; }
    float getRuntimeTargetTemp() const override { return runtimeTargetTemp; }
    uint32_t getRuntimeTargetTime() const override { return runtimeTargetTime; }
    PresetType getRuntimePreset() const override { return runtimePreset; }
};

#endif


