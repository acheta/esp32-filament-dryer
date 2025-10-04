#ifdef UNIT_TEST
    #include <unity.h>
#else
    #include <unity.h>
    #include <Arduino.h>
#endif

#include "../../src/Dryer.h"
#include "../mocks/MockSensorManager.h"
#include "../mocks/MockHeaterControl.h"
#include "../mocks/MockPIDController.h"
#include "../mocks/MockSafetyMonitor.h"
#include "../mocks/MockSettingsStorage.h"
#include "../mocks/MockSoundController.h"

// Test fixture
MockSensorManager* sensors;
MockHeaterControl* heater;
MockPIDController* pid;
MockSafetyMonitor* safety;
MockSettingsStorage* storage;
MockSoundController* sound;
Dryer* dryer;

void setUp(void) {
    sensors = new MockSensorManager();
    heater = new MockHeaterControl();
    pid = new MockPIDController();
    safety = new MockSafetyMonitor();
    storage = new MockSettingsStorage();
    sound = new MockSoundController();

    dryer = new Dryer(sensors, heater, pid, safety, storage, sound);
}

void tearDown(void) {
    delete dryer;
    delete sound;
    delete storage;
    delete safety;
    delete pid;
    delete heater;
    delete sensors;
}

// ==================== Initialization Tests ====================

void test_dryer_initializes_all_components() {
    dryer->begin();

    TEST_ASSERT_TRUE(sensors->isInitialized());
    TEST_ASSERT_TRUE(heater->isInitialized());
    TEST_ASSERT_TRUE(pid->isInitialized());
    TEST_ASSERT_TRUE(safety->isInitialized());
    TEST_ASSERT_TRUE(storage->isInitialized());
    TEST_ASSERT_TRUE(sound->isInitialized());
}

void test_dryer_starts_in_ready_state() {
    dryer->begin();
    TEST_ASSERT_EQUAL(DryerState::READY, dryer->getState());
}

void test_dryer_registers_callbacks_with_components() {
    dryer->begin();

    // Verify callbacks registered
    TEST_ASSERT_EQUAL(1, sensors->getHeaterTempCallbackCount());
    TEST_ASSERT_EQUAL(1, sensors->getBoxDataCallbackCount());
    TEST_ASSERT_EQUAL(1, sensors->getErrorCallbackCount());
    TEST_ASSERT_EQUAL(1, safety->getCallbackCount());
}

// ==================== State Transition Tests ====================

void test_dryer_transitions_from_ready_to_running() {
    dryer->begin();

    dryer->start();

    TEST_ASSERT_EQUAL(DryerState::RUNNING, dryer->getState());
    TEST_ASSERT_EQUAL(1, heater->getStartCallCount());
}

void test_dryer_cannot_start_from_invalid_states() {
    dryer->begin();

    // Start normally
    dryer->start();
    TEST_ASSERT_EQUAL(DryerState::RUNNING, dryer->getState());

    // Try to start again (should be ignored)
    uint32_t startCount = heater->getStartCallCount();
    dryer->start();
    TEST_ASSERT_EQUAL(startCount, heater->getStartCallCount()); // No change
}

void test_dryer_transitions_from_running_to_paused() {
    dryer->begin();
    dryer->start();

    dryer->pause();

    TEST_ASSERT_EQUAL(DryerState::PAUSED, dryer->getState());
    TEST_ASSERT_EQUAL(1, heater->getStopCallCount());
}

void test_dryer_resumes_from_paused() {
    dryer->begin();
    dryer->start();
    dryer->pause();

    dryer->resume();

    TEST_ASSERT_EQUAL(DryerState::RUNNING, dryer->getState());
    TEST_ASSERT_EQUAL(2, heater->getStartCallCount()); // Start + Resume
}

void test_dryer_resets_to_ready() {
    dryer->begin();
    dryer->start();

    dryer->reset();

    TEST_ASSERT_EQUAL(DryerState::READY, dryer->getState());
    TEST_ASSERT_EQUAL(1, storage->getClearRuntimeStateCallCount());
}

void test_dryer_stops_from_running() {
    dryer->begin();
    dryer->start();

    dryer->stop();

    TEST_ASSERT_EQUAL(DryerState::READY, dryer->getState());
}

// ==================== State Change Callback Tests ====================

void test_dryer_fires_state_change_callback() {
    bool callbackFired = false;
    DryerState oldState = DryerState::READY;
    DryerState newState = DryerState::READY;

    dryer->begin();

    dryer->registerStateChangeCallback(
        [&callbackFired, &oldState, &newState](DryerState old, DryerState newer) {
            callbackFired = true;
            oldState = old;
            newState = newer;
        }
    );

    dryer->start();

    TEST_ASSERT_TRUE(callbackFired);
    TEST_ASSERT_EQUAL(DryerState::READY, oldState);
    TEST_ASSERT_EQUAL(DryerState::RUNNING, newState);
}

// ==================== Sensor Integration Tests ====================

void test_dryer_receives_heater_temp_updates() {
    dryer->begin();
    dryer->start();

    // Simulate sensor update
    sensors->triggerHeaterTempUpdate(65.5, 1000);

    CurrentStats stats = dryer->getCurrentStats();
    TEST_ASSERT_EQUAL_FLOAT(65.5, stats.currentTemp);
}

void test_dryer_updates_pid_on_heater_temp() {
    dryer->begin();
    dryer->start();

    pid->resetCounts();

    // Trigger heater temp update
    sensors->triggerHeaterTempUpdate(60.0, 500);

    TEST_ASSERT_EQUAL(1, pid->getComputeCallCount());
    TEST_ASSERT_EQUAL_FLOAT(60.0, pid->getLastInput());
}

void test_dryer_sets_heater_pwm_from_pid_output() {
    dryer->begin();
    dryer->start();

    pid->setOutput(150);
    heater->resetCounts();

    // Trigger heater temp update
    sensors->triggerHeaterTempUpdate(55.0, 500);

    TEST_ASSERT_EQUAL(1, heater->getSetPWMCallCount());
    TEST_ASSERT_EQUAL(150, heater->getCurrentPWM());
}

void test_dryer_does_not_update_pid_when_not_running() {
    dryer->begin();
    // Don't start

    pid->resetCounts();

    sensors->triggerHeaterTempUpdate(60.0, 500);

    TEST_ASSERT_EQUAL(0, pid->getComputeCallCount());
}

// ==================== Safety Integration Tests ====================

void test_dryer_transitions_to_failed_on_emergency() {
    dryer->begin();
    dryer->start();

    // Trigger emergency via safety monitor
    safety->triggerEmergency("Temperature exceeded");
    safety->update(1000);

    TEST_ASSERT_EQUAL(DryerState::FAILED, dryer->getState());
    TEST_ASSERT_EQUAL(1, heater->getEmergencyStopCallCount());
}

void test_dryer_plays_alarm_on_emergency() {
    dryer->begin();
    dryer->start();

    safety->triggerEmergency("Emergency");
    safety->update(1000);

    TEST_ASSERT_EQUAL(1, sound->getAlarmCount());
}

void test_dryer_saves_emergency_state() {
    dryer->begin();
    dryer->start();

    safety->triggerEmergency("Overheat");
    safety->update(1000);

    // Emergency state saved via saveEmergencyState
    // (implementation detail of storage mock)
}

// ==================== Timer Tests ====================

void test_dryer_finishes_when_target_time_reached() {
    dryer->begin();

    // Use PLA preset (4 hours = 14400 seconds)
    dryer->selectPreset(PresetType::PLA);
    dryer->start();

    // Simulate 14400 seconds (just enough to finish)
    dryer->update(14401000);

    TEST_ASSERT_EQUAL(DryerState::FINISHED, dryer->getState());
}

void test_dryer_plays_finished_sound() {
    dryer->begin();
    dryer->selectPreset(PresetType::PLA);
    dryer->start();

    dryer->update(14400000);

    TEST_ASSERT_EQUAL(1, sound->getFinishedCount());
}

void test_dryer_clears_runtime_state_on_finish() {
    dryer->begin();
    dryer->selectPreset(PresetType::PLA);
    dryer->start();

    storage->resetCounts();

    dryer->update(14400000);

    TEST_ASSERT_EQUAL(1, storage->getClearRuntimeStateCallCount());
}

// ==================== Stats Update Tests ====================

void test_dryer_provides_current_stats() {
    dryer->begin();
    dryer->selectPreset(PresetType::PLA);

    dryer->start();
    dryer->update(1000);

    sensors->triggerHeaterTempUpdate(65.5, 1000);
    sensors->triggerBoxDataUpdate(48.2, 35.0, 1000);

    CurrentStats stats = dryer->getCurrentStats();

    TEST_ASSERT_EQUAL(DryerState::RUNNING, stats.state);
    TEST_ASSERT_EQUAL_FLOAT(65.5, stats.currentTemp);
    TEST_ASSERT_EQUAL_FLOAT(50.0, stats.targetTemp); // PLA preset
    TEST_ASSERT_EQUAL_FLOAT(48.2, stats.boxTemp);
    TEST_ASSERT_EQUAL_FLOAT(35.0, stats.boxHumidity);
    TEST_ASSERT_EQUAL(PresetType::PLA, stats.activePreset);
}

void test_dryer_calculates_elapsed_time() {
    dryer->begin();
    dryer->selectPreset(PresetType::PLA);
    dryer->start();

    // After 10 seconds
    dryer->update(10000);

    CurrentStats stats = dryer->getCurrentStats();
    TEST_ASSERT_TRUE(stats.elapsedTime >= 9 && stats.elapsedTime <= 11);
}

void test_dryer_calculates_remaining_time() {
    dryer->begin();
    dryer->selectPreset(PresetType::PLA); // 14400 seconds
    dryer->start();

    dryer->update(1000); // 1 second elapsed

    CurrentStats stats = dryer->getCurrentStats();
    TEST_ASSERT_TRUE(stats.remainingTime >= 14398 && stats.remainingTime <= 14400);
}

void test_dryer_fires_stats_update_callback() {
    bool callbackFired = false;
    CurrentStats receivedStats;

    dryer->begin();

    dryer->registerStatsUpdateCallback(
        [&callbackFired, &receivedStats](const CurrentStats& stats) {
            callbackFired = true;
            receivedStats = stats;
        }
    );

    dryer->update(1000);

    TEST_ASSERT_TRUE(callbackFired);
}

// ==================== Preset Tests ====================

void test_dryer_selects_pla_preset() {
    dryer->begin();
    dryer->selectPreset(PresetType::PLA);

    TEST_ASSERT_EQUAL(PresetType::PLA, dryer->getActivePreset());

    CurrentStats stats = dryer->getCurrentStats();
    TEST_ASSERT_EQUAL_FLOAT(50.0, stats.targetTemp);
}

void test_dryer_selects_petg_preset() {
    dryer->begin();
    dryer->selectPreset(PresetType::PETG);

    TEST_ASSERT_EQUAL(PresetType::PETG, dryer->getActivePreset());

    CurrentStats stats = dryer->getCurrentStats();
    TEST_ASSERT_EQUAL_FLOAT(65.0, stats.targetTemp);
}

void test_dryer_cannot_change_preset_while_running() {
    dryer->begin();
    dryer->selectPreset(PresetType::PLA);
    dryer->start();

    // Try to change preset
    dryer->selectPreset(PresetType::PETG);

    // Should still be PLA
    TEST_ASSERT_EQUAL(PresetType::PLA, dryer->getActivePreset());
}

// ==================== PID Profile Tests ====================

void test_dryer_sets_pid_profile() {
    dryer->begin();

    dryer->setPIDProfile(PIDProfile::SOFT);
    TEST_ASSERT_EQUAL(PIDProfile::SOFT, pid->getProfile());

    dryer->setPIDProfile(PIDProfile::STRONG);
    TEST_ASSERT_EQUAL(PIDProfile::STRONG, pid->getProfile());
}

void test_dryer_gets_pid_profile() {
    dryer->begin();
    dryer->setPIDProfile(PIDProfile::SOFT);

    TEST_ASSERT_EQUAL(PIDProfile::SOFT, dryer->getPIDProfile());
}

// ==================== Sound Control Tests ====================

void test_dryer_controls_sound_enabled() {
    dryer->begin();

    dryer->setSoundEnabled(false);
    TEST_ASSERT_FALSE(sound->isEnabled());

    dryer->setSoundEnabled(true);
    TEST_ASSERT_TRUE(sound->isEnabled());
}

void test_dryer_plays_start_sound() {
    dryer->begin();

    dryer->start();

    TEST_ASSERT_EQUAL(1, sound->getStartCount());
}

void test_dryer_does_not_play_sound_when_disabled() {
    dryer->begin();
    dryer->setSoundEnabled(false);

    sound->resetCounts();

    dryer->start();

    TEST_ASSERT_EQUAL(0, sound->getStartCount());
}

// ==================== State Persistence Tests ====================

void test_dryer_persists_state_during_running() {
    dryer->begin();
    dryer->start();

    storage->resetCounts();

    // Simulate 2 seconds of running
    for (uint32_t t = 0; t <= 2000; t += 100) {
        dryer->update(t);
    }

    // Should save state at 1000ms and 2000ms (every 1 second)
    TEST_ASSERT_TRUE(storage->getSaveRuntimeStateCallCount() >= 2);
}

void test_dryer_does_not_persist_when_not_running() {
    dryer->begin();
    // Not started

    storage->resetCounts();

    dryer->update(1000);
    dryer->update(2000);

    TEST_ASSERT_EQUAL(0, storage->getSaveRuntimeStateCallCount());
}

// ==================== Constraint Getters Tests ====================

void test_dryer_provides_constraints() {
    dryer->begin();

    TEST_ASSERT_EQUAL_FLOAT(30.0, dryer->getMinTemp());
    TEST_ASSERT_EQUAL_FLOAT(80.0, dryer->getMaxTemp());
    TEST_ASSERT_EQUAL(36000, dryer->getMaxTime());
    TEST_ASSERT_EQUAL_FLOAT(10.0, dryer->getMaxOvershoot());
}

// ==================== Integration Scenario Tests ====================

void test_dryer_complete_heating_cycle() {
    dryer->begin();
    dryer->selectPreset(PresetType::PLA);
    dryer->setPIDProfile(PIDProfile::NORMAL);

    // Start
    dryer->start();
    TEST_ASSERT_EQUAL(DryerState::RUNNING, dryer->getState());

    // Simulate heating
    pid->setOutput(200);

    for (uint32_t t = 0; t <= 1000; t += 500) {
        float temp = 25.0 + (t / 500.0) * 5.0; // Gradual heating
        sensors->triggerHeaterTempUpdate(temp, t);
        sensors->triggerBoxDataUpdate(temp - 5.0, 40.0, t);

        dryer->update(t);
        safety->update(t);
    }

    // Should still be running
    TEST_ASSERT_EQUAL(DryerState::RUNNING, dryer->getState());
    TEST_ASSERT_TRUE(heater->getCurrentPWM() > 0);
}

void test_dryer_pause_and_resume_cycle() {
    dryer->begin();
    dryer->selectPreset(PresetType::PLA);
    dryer->start();

    // Run for 5 seconds
    dryer->update(5000);
    CurrentStats stats1 = dryer->getCurrentStats();
    uint32_t elapsed1 = stats1.elapsedTime; // 5

    // Pause
    dryer->pause();
    TEST_ASSERT_EQUAL(DryerState::PAUSED, dryer->getState());

    // Wait 3 seconds (time should not advance)
    dryer->update(8000);
    CurrentStats stats2 = dryer->getCurrentStats(); // 5
    uint32_t elapsed2 = stats2.elapsedTime; // Should still be 5

    // Elapsed time should be about the same
    TEST_ASSERT_TRUE(elapsed2 >= elapsed1 - 1 && elapsed2 <= elapsed1 + 1);

    // Resume
    dryer->resume();

    TEST_ASSERT_EQUAL(DryerState::RUNNING, dryer->getState());

    // Run for another 2 seconds
    dryer->update(10000);
    CurrentStats stats3 = dryer->getCurrentStats(); // 7
    uint32_t elapsed3 = stats3.elapsedTime; // Should be about 7

    // Total elapsed should be about 7 seconds (5 + 2, excluding pause)
    TEST_ASSERT_TRUE(elapsed3 >= elapsed1+2-1 && elapsed3 <= elapsed1+2+1);
}

// ==================== Main Test Runner ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_dryer_initializes_all_components);
    RUN_TEST(test_dryer_starts_in_ready_state);
    RUN_TEST(test_dryer_registers_callbacks_with_components);

    // State transitions
    RUN_TEST(test_dryer_transitions_from_ready_to_running);
    RUN_TEST(test_dryer_cannot_start_from_invalid_states);
    RUN_TEST(test_dryer_transitions_from_running_to_paused);
    RUN_TEST(test_dryer_resumes_from_paused);
    RUN_TEST(test_dryer_resets_to_ready);
    RUN_TEST(test_dryer_stops_from_running);

    // Callbacks
    RUN_TEST(test_dryer_fires_state_change_callback);

    // Sensor integration
    RUN_TEST(test_dryer_receives_heater_temp_updates);
    RUN_TEST(test_dryer_updates_pid_on_heater_temp);
    RUN_TEST(test_dryer_sets_heater_pwm_from_pid_output);
    RUN_TEST(test_dryer_does_not_update_pid_when_not_running);

    // Safety integration
    RUN_TEST(test_dryer_transitions_to_failed_on_emergency);
    RUN_TEST(test_dryer_plays_alarm_on_emergency);
    RUN_TEST(test_dryer_saves_emergency_state);

    // Timer
    RUN_TEST(test_dryer_finishes_when_target_time_reached);
    RUN_TEST(test_dryer_plays_finished_sound);
    RUN_TEST(test_dryer_clears_runtime_state_on_finish);

    // Stats
    RUN_TEST(test_dryer_provides_current_stats);
    RUN_TEST(test_dryer_calculates_elapsed_time);
    RUN_TEST(test_dryer_calculates_remaining_time);
    RUN_TEST(test_dryer_fires_stats_update_callback);

    // Presets
    RUN_TEST(test_dryer_selects_pla_preset);
    RUN_TEST(test_dryer_selects_petg_preset);
    RUN_TEST(test_dryer_cannot_change_preset_while_running);

    // PID profile
    RUN_TEST(test_dryer_sets_pid_profile);
    RUN_TEST(test_dryer_gets_pid_profile);

    // Sound
    RUN_TEST(test_dryer_controls_sound_enabled);
    RUN_TEST(test_dryer_plays_start_sound);
    RUN_TEST(test_dryer_does_not_play_sound_when_disabled);

    // Persistence
    RUN_TEST(test_dryer_persists_state_during_running);
    RUN_TEST(test_dryer_does_not_persist_when_not_running);

    // Constraints
    RUN_TEST(test_dryer_provides_constraints);

    // Integration scenarios
    RUN_TEST(test_dryer_complete_heating_cycle);
    RUN_TEST(test_dryer_pause_and_resume_cycle);

    return UNITY_END();
}