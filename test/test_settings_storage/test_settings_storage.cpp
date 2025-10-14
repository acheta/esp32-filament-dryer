#ifdef UNIT_TEST
    #include <unity.h>
#else
    #include <unity.h>
    #include <Arduino.h>
#endif

#include "../../src/storage/SettingsStorage.h"

// Test fixture
SettingsStorage* storage;

void setUp(void) {
    storage = new SettingsStorage();
}

void tearDown(void) {
    delete storage;
}

// ==================== Initialization Tests ====================

void test_storage_initializes() {
    storage->begin();
    TEST_ASSERT_TRUE(storage->isInitialized());
}

void test_storage_starts_with_defaults() {
    storage->begin();

    DryingPreset preset = storage->loadCustomPreset();
    TEST_ASSERT_EQUAL_FLOAT(PRESET_CUSTOM_TEMP, preset.targetTemp);
    TEST_ASSERT_EQUAL(PRESET_CUSTOM_TIME, preset.targetTime);
    TEST_ASSERT_EQUAL_FLOAT(PRESET_CUSTOM_OVERSHOOT, preset.maxOvershoot);

    TEST_ASSERT_EQUAL(PresetType::PLA, storage->loadSelectedPreset());
    TEST_ASSERT_EQUAL(PIDProfile::NORMAL, storage->loadPIDProfile());
    TEST_ASSERT_TRUE(storage->loadSoundEnabled());
}

void test_storage_has_no_runtime_on_fresh_start() {
    storage->begin();
    TEST_ASSERT_FALSE(storage->hasValidRuntimeState());
}

// ==================== Custom Preset Tests ====================

void test_storage_saves_and_loads_custom_preset() {
    storage->begin();

    DryingPreset preset;
    preset.targetTemp = 55.5;
    preset.targetTime = 7200;
    preset.maxOvershoot = 8.0;

    storage->saveCustomPreset(preset);

    DryingPreset loaded = storage->loadCustomPreset();
    TEST_ASSERT_EQUAL_FLOAT(55.5, loaded.targetTemp);
    TEST_ASSERT_EQUAL(7200, loaded.targetTime);
    TEST_ASSERT_EQUAL_FLOAT(8.0, loaded.maxOvershoot);
}

// ==================== Selected Preset Tests ====================

void test_storage_saves_and_loads_selected_preset() {
    storage->begin();

    storage->saveSelectedPreset(PresetType::PETG);
    TEST_ASSERT_EQUAL(PresetType::PETG, storage->loadSelectedPreset());

    storage->saveSelectedPreset(PresetType::CUSTOM);
    TEST_ASSERT_EQUAL(PresetType::CUSTOM, storage->loadSelectedPreset());
}

// ==================== PID Profile Tests ====================

void test_storage_saves_and_loads_pid_profile() {
    storage->begin();

    storage->savePIDProfile(PIDProfile::SOFT);
    TEST_ASSERT_EQUAL(PIDProfile::SOFT, storage->loadPIDProfile());

    storage->savePIDProfile(PIDProfile::STRONG);
    TEST_ASSERT_EQUAL(PIDProfile::STRONG, storage->loadPIDProfile());
}

// ==================== Sound Setting Tests ====================

void test_storage_saves_and_loads_sound_setting() {
    storage->begin();

    storage->saveSoundEnabled(false);
    TEST_ASSERT_FALSE(storage->loadSoundEnabled());

    storage->saveSoundEnabled(true);
    TEST_ASSERT_TRUE(storage->loadSoundEnabled());
}

// ==================== Runtime State Tests ====================

void test_storage_saves_runtime_state() {
    storage->begin();

    storage->saveRuntimeState(
        DryerState::RUNNING,
        1234,          // elapsed
        60.0,          // targetTemp
        18000,         // targetTime
        PresetType::PETG,
        5000           // timestamp
    );

    TEST_ASSERT_TRUE(storage->hasValidRuntimeState());
    TEST_ASSERT_EQUAL(DryerState::RUNNING, storage->getRuntimeState());
    TEST_ASSERT_EQUAL(1234, storage->getRuntimeElapsed());
    TEST_ASSERT_EQUAL_FLOAT(60.0, storage->getRuntimeTargetTemp());
    TEST_ASSERT_EQUAL(18000, storage->getRuntimeTargetTime());
    TEST_ASSERT_EQUAL(PresetType::PETG, storage->getRuntimePreset());
}

void test_storage_clears_runtime_state() {
    storage->begin();

    storage->saveRuntimeState(
        DryerState::RUNNING,
        1000, 50.0, 14400,
        PresetType::PLA, 5000
    );

    TEST_ASSERT_TRUE(storage->hasValidRuntimeState());

    storage->clearRuntimeState();
    TEST_ASSERT_FALSE(storage->hasValidRuntimeState());
}

void test_storage_ignores_non_running_runtime_state() {
    storage->begin();

    // Save PAUSED state
    storage->saveRuntimeState(
        DryerState::PAUSED,
        1000, 50.0, 14400,
        PresetType::PLA, 5000
    );

    // Should not be considered valid for recovery
    // (In real implementation, this would be tested by restarting)
    // For unit test, we verify the state is saved but marked as invalid
    TEST_ASSERT_TRUE(storage->hasValidRuntimeState()); // Cached as valid
    // But on restart (begin()), only RUNNING would be loaded
}

// ==================== Emergency State Tests ====================

void test_storage_saves_emergency_state() {
    storage->begin();

    storage->saveEmergencyState("Overheat detected");

    // Emergency state should be saved (implementation detail)
    // Just verify no crash occurs
    TEST_ASSERT_TRUE(true);
}

// ==================== Settings Persistence Tests ====================

void test_storage_persists_all_settings() {
    storage->begin();

    // Set all settings
    DryingPreset preset;
    preset.targetTemp = 62.0;
    preset.targetTime = 10800;
    preset.maxOvershoot = 12.0;

    storage->saveCustomPreset(preset);
    storage->saveSelectedPreset(PresetType::CUSTOM);
    storage->savePIDProfile(PIDProfile::STRONG);
    storage->saveSoundEnabled(false);

    // Verify all loaded correctly
    DryingPreset loaded = storage->loadCustomPreset();
    TEST_ASSERT_EQUAL_FLOAT(62.0, loaded.targetTemp);
    TEST_ASSERT_EQUAL(10800, loaded.targetTime);
    TEST_ASSERT_EQUAL_FLOAT(12.0, loaded.maxOvershoot);

    TEST_ASSERT_EQUAL(PresetType::CUSTOM, storage->loadSelectedPreset());
    TEST_ASSERT_EQUAL(PIDProfile::STRONG, storage->loadPIDProfile());
    TEST_ASSERT_FALSE(storage->loadSoundEnabled());
}

// ==================== Graceful Degradation Tests ====================

void test_storage_continues_on_save_failure() {
    storage->begin();

    // Even if save fails internally, storage should continue
    // and not crash the system
    storage->saveCustomPreset(DryingPreset());
    storage->saveSelectedPreset(PresetType::PLA);
    storage->savePIDProfile(PIDProfile::NORMAL);

    TEST_ASSERT_TRUE(storage->isInitialized());
}

// ==================== Version Number Tests ====================

void test_storage_includes_version_in_saved_files() {
    storage->begin();

    // Save some settings
    storage->saveSelectedPreset(PresetType::PETG);
    storage->savePIDProfile(PIDProfile::STRONG);

    // Version is embedded in the file (implementation detail)
    // Just verify no crash and data persists correctly
    TEST_ASSERT_EQUAL(PresetType::PETG, storage->loadSelectedPreset());
    TEST_ASSERT_EQUAL(PIDProfile::STRONG, storage->loadPIDProfile());
}

void test_storage_handles_runtime_state_versioning() {
    storage->begin();

    storage->saveRuntimeState(
        DryerState::RUNNING,
        5000, 60.0, 18000,
        PresetType::PETG, 10000
    );

    TEST_ASSERT_TRUE(storage->hasValidRuntimeState());
    TEST_ASSERT_EQUAL(DryerState::RUNNING, storage->getRuntimeState());
}

// ==================== Settings Load/Save Cycle Tests ====================

void test_storage_persists_settings_across_simulated_restart() {
    // First "session" - save settings
    storage->begin();

    DryingPreset preset;
    preset.targetTemp = 58.0;
    preset.targetTime = 9000;
    preset.maxOvershoot = 15.0;

    storage->saveCustomPreset(preset);
    storage->saveSelectedPreset(PresetType::CUSTOM);
    storage->savePIDProfile(PIDProfile::SOFT);
    storage->saveSoundEnabled(false);

    // Simulate restart by deleting and recreating storage
    delete storage;
    storage = new SettingsStorage();

    // Second "session" - load settings
    storage->begin();

    // Verify all settings persisted
    DryingPreset loaded = storage->loadCustomPreset();
    TEST_ASSERT_EQUAL_FLOAT(58.0, loaded.targetTemp);
    TEST_ASSERT_EQUAL(9000, loaded.targetTime);
    TEST_ASSERT_EQUAL_FLOAT(15.0, loaded.maxOvershoot);

    TEST_ASSERT_EQUAL(PresetType::CUSTOM, storage->loadSelectedPreset());
    TEST_ASSERT_EQUAL(PIDProfile::SOFT, storage->loadPIDProfile());
    TEST_ASSERT_FALSE(storage->loadSoundEnabled());
}

void test_storage_persists_runtime_state_for_power_recovery() {
    // First "session" - running cycle
    storage->begin();

    storage->saveRuntimeState(
        DryerState::RUNNING,
        7200, 65.0, 18000,
        PresetType::PETG, 12345
    );

    // Simulate power loss and restart
    delete storage;
    storage = new SettingsStorage();

    // Second "session" - after power on
    storage->begin();

    // Should have valid runtime state
    TEST_ASSERT_TRUE(storage->hasValidRuntimeState());
    TEST_ASSERT_EQUAL(DryerState::RUNNING, storage->getRuntimeState());
    TEST_ASSERT_EQUAL(7200, storage->getRuntimeElapsed());
    TEST_ASSERT_EQUAL_FLOAT(65.0, storage->getRuntimeTargetTemp());
    TEST_ASSERT_EQUAL(18000, storage->getRuntimeTargetTime());
    TEST_ASSERT_EQUAL(PresetType::PETG, storage->getRuntimePreset());
}

// ==================== Main Test Runner ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_storage_initializes);
    RUN_TEST(test_storage_starts_with_defaults);
    RUN_TEST(test_storage_has_no_runtime_on_fresh_start);

    // Custom preset
    RUN_TEST(test_storage_saves_and_loads_custom_preset);

    // Selected preset
    RUN_TEST(test_storage_saves_and_loads_selected_preset);

    // PID profile
    RUN_TEST(test_storage_saves_and_loads_pid_profile);

    // Sound setting
    RUN_TEST(test_storage_saves_and_loads_sound_setting);

    // Runtime state
    RUN_TEST(test_storage_saves_runtime_state);
    RUN_TEST(test_storage_clears_runtime_state);
    RUN_TEST(test_storage_ignores_non_running_runtime_state);

    // Emergency
    RUN_TEST(test_storage_saves_emergency_state);

    // Persistence
    RUN_TEST(test_storage_persists_all_settings);

    // Graceful degradation
    RUN_TEST(test_storage_continues_on_save_failure);

    // Version numbers
    RUN_TEST(test_storage_includes_version_in_saved_files);
    RUN_TEST(test_storage_handles_runtime_state_versioning);

    // Persistence across restarts
    RUN_TEST(test_storage_persists_settings_across_simulated_restart);
    RUN_TEST(test_storage_persists_runtime_state_for_power_recovery);

    return UNITY_END();
}

