#ifdef UNIT_TEST
    #include <unity.h>
#else
    #include <unity.h>
    #include <Arduino.h>
#endif

#include "../../src/userInterface/UIController.h"
#include "../mocks/MockDisplay.h"
#include "../mocks/MockMenuController.h"
#include "../mocks/MockButtonManager.h"
#include "../mocks/MockSoundController.h"
#include "../mocks/MockDryer.h"

// Test fixture
UIController* uiController;
MockDisplay* mockDisplay;
MockMenuController* mockMenu;
MockButtonManager* mockButtons;
MockSoundController* mockSound;
MockDryer* mockDryer;

void setUp(void) {
    mockDisplay = new MockDisplay();
    mockMenu = new MockMenuController();
    mockButtons = new MockButtonManager();
    mockSound = new MockSoundController();
    mockDryer = new MockDryer();

    uiController = new UIController(
        mockDisplay,
        mockMenu,
        mockButtons,
        mockSound,
        mockDryer
    );
}

void tearDown(void) {
    delete uiController;
    delete mockDisplay;
    delete mockMenu;
    delete mockButtons;
    delete mockSound;
    delete mockDryer;
}

// ==================== Initialization Tests ====================

void test_begin_initializes_all_components() {
    uiController->begin();

    // Should register callbacks with button manager (3 buttons)
    TEST_ASSERT_EQUAL(3, mockButtons->getCallbackCount());

    // Should register selection callback with menu
    TEST_ASSERT_EQUAL(1, mockMenu->getCallbackCount());

    // Should register stats callback with dryer
    TEST_ASSERT_EQUAL(1, mockDryer->getStatsCallbackCount());
}

void test_begin_sets_menu_constraints() {
    uiController->begin();

    // Menu should have constraints set from dryer
    TEST_ASSERT_TRUE(mockMenu->wasSetConstraintsCalled());
}

void test_begin_initializes_custom_preset() {
    uiController->begin();

    // Menu should have custom preset values set
    TEST_ASSERT_TRUE(mockMenu->wasSetCustomPresetValuesCalled());
}

void test_begin_sets_pid_profile() {
    uiController->begin();

    // Menu should have PID profile set
    TEST_ASSERT_TRUE(mockMenu->wasSetPIDProfileCalled());
}

void test_begin_starts_in_home_mode() {
    uiController->begin();

    TEST_ASSERT_FALSE(uiController->isInMenuMode());
}

// ==================== Button Callback Tests ====================

void test_set_button_single_click_enters_menu_from_home() {
    uiController->begin();
    mockDisplay->resetCounts();

    // Simulate SET button single click
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);

    // Should enter menu mode
    TEST_ASSERT_TRUE(uiController->isInMenuMode());
}

void test_set_button_long_press_toggles_dryer_from_home() {
    uiController->begin();
    mockDryer->setState(DryerState::READY);

    // Simulate SET button long press
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::LONG_PRESS);

    // Should call dryer start
    TEST_ASSERT_EQUAL(1, mockDryer->getStartCallCount());
}

void test_set_button_long_press_pauses_when_running() {
    uiController->begin();
    mockDryer->setState(DryerState::RUNNING);

    // Simulate SET button long press
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::LONG_PRESS);

    // Should pause
    TEST_ASSERT_EQUAL(1, mockDryer->getPauseCallCount());
}

void test_set_button_long_press_resumes_when_paused() {
    uiController->begin();
    mockDryer->setState(DryerState::PAUSED);

    // Simulate SET button long press
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::LONG_PRESS);

    // Should resume
    TEST_ASSERT_EQUAL(1, mockDryer->getResumeCallCount());
}

void test_set_button_in_menu_sends_action() {
    uiController->begin();

    // Enter menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);
    TEST_ASSERT_TRUE(uiController->isInMenuMode());

    mockMenu->resetCallCounts();

    // Single click in menu should send action to menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);

    TEST_ASSERT_EQUAL(1, mockMenu->getHandleActionCallCount());
}

void test_set_button_long_press_in_menu_exits_at_root() {
    uiController->begin();

    // Enter menu (starts at ROOT)
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);
    TEST_ASSERT_TRUE(uiController->isInMenuMode());

    // Long press at ROOT should exit menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::LONG_PRESS);

    TEST_ASSERT_FALSE(uiController->isInMenuMode());
}

void test_set_button_long_press_in_submenu_sends_action() {
    uiController->begin();

    // Enter menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);

    // Simulate being in submenu (not ROOT)
    mockMenu->setCurrentMenuPath(MenuPath::STATUS);
    mockMenu->resetCallCounts();

    // Long press should send action
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::LONG_PRESS);

    TEST_ASSERT_EQUAL(1, mockMenu->getHandleActionCallCount());
}

void test_up_button_cycles_stats_screen_forward() {
    uiController->begin();

    auto initialScreen = uiController->getCurrentStatsScreen();

    mockDisplay->resetCounts();

    // Click UP
    mockButtons->simulateButtonEvent(ButtonType::UP, ButtonEvent::SINGLE_CLICK);

    // Call update to render
    uiController->update(1000);

    // Should trigger display updates (dirty flag)
    TEST_ASSERT_GREATER_THAN(0, mockDisplay->getDisplayCallCount());
}

void test_down_button_cycles_stats_screen_backward() {
    uiController->begin();

    mockDisplay->resetCounts();

    // Click DOWN
    mockButtons->simulateButtonEvent(ButtonType::DOWN, ButtonEvent::SINGLE_CLICK);

    // Call update to render
    uiController->update(1000);

    // Should trigger display update
    TEST_ASSERT_GREATER_THAN(0, mockDisplay->getDisplayCallCount());
}

void test_up_button_in_menu_sends_action() {
    uiController->begin();

    // Enter menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);
    mockMenu->resetCallCounts();

    // UP in menu
    mockButtons->simulateButtonEvent(ButtonType::UP, ButtonEvent::SINGLE_CLICK);

    TEST_ASSERT_EQUAL(1, mockMenu->getHandleActionCallCount());
}

void test_down_button_in_menu_sends_action() {
    uiController->begin();

    // Enter menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);
    mockMenu->resetCallCounts();

    // DOWN in menu
    mockButtons->simulateButtonEvent(ButtonType::DOWN, ButtonEvent::SINGLE_CLICK);

    TEST_ASSERT_EQUAL(1, mockMenu->getHandleActionCallCount());
}

// ==================== Menu Selection Tests ====================

void test_menu_selection_start_calls_dryer_start() {
    uiController->begin();

    // Simulate menu selection for START
    mockMenu->fireSelectionCallback(MenuPath::STATUS_START, 0);

    TEST_ASSERT_EQUAL(1, mockDryer->getStartCallCount());
}

void test_menu_selection_start_exits_menu() {
    uiController->begin();

    // Enter menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);
    TEST_ASSERT_TRUE(uiController->isInMenuMode());

    // Select START
    mockMenu->fireSelectionCallback(MenuPath::STATUS_START, 0);

    TEST_ASSERT_FALSE(uiController->isInMenuMode());
}

void test_menu_selection_pause_calls_dryer_pause() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::STATUS_PAUSE, 0);

    TEST_ASSERT_EQUAL(1, mockDryer->getPauseCallCount());
}

void test_menu_selection_reset_calls_dryer_reset() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::STATUS_RESET, 0);

    TEST_ASSERT_EQUAL(1, mockDryer->getResetCallCount());
}

void test_menu_selection_preset_pla() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::PRESET_PLA, 0);

    TEST_ASSERT_EQUAL(PresetType::PLA, mockDryer->getActivePreset());
}

void test_menu_selection_preset_petg() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::PRESET_PETG, 0);

    TEST_ASSERT_EQUAL(PresetType::PETG, mockDryer->getActivePreset());
}

void test_menu_selection_preset_custom() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::PRESET_CUSTOM, 0);

    TEST_ASSERT_EQUAL(PresetType::CUSTOM, mockDryer->getActivePreset());
}

void test_menu_selection_custom_temp() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::CUSTOM_TEMP, 55);

    DryingPreset preset = mockDryer->getCustomPreset();
    TEST_ASSERT_EQUAL_FLOAT(55.0, preset.targetTemp);
}

void test_menu_selection_custom_time() {
    uiController->begin();

    // Menu sends time in minutes, dryer expects seconds
    mockMenu->fireSelectionCallback(MenuPath::CUSTOM_TIME, 120);

    DryingPreset preset = mockDryer->getCustomPreset();
    TEST_ASSERT_EQUAL(7200, preset.targetTime);
}

void test_menu_selection_custom_overshoot() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::CUSTOM_OVERSHOOT, 8);

    DryingPreset preset = mockDryer->getCustomPreset();
    TEST_ASSERT_EQUAL_FLOAT(8.0, preset.maxOvershoot);
}

void test_menu_selection_pid_soft() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::PID_SOFT, 0);

    TEST_ASSERT_EQUAL(PIDProfile::SOFT, mockDryer->getPIDProfile());
}

void test_menu_selection_pid_normal() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::PID_NORMAL, 0);

    TEST_ASSERT_EQUAL(PIDProfile::NORMAL, mockDryer->getPIDProfile());
}

void test_menu_selection_pid_strong() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::PID_STRONG, 0);

    TEST_ASSERT_EQUAL(PIDProfile::STRONG, mockDryer->getPIDProfile());
}

void test_menu_selection_sound_on() {
    uiController->begin();

    mockDryer->setSoundEnabled(false);

    mockMenu->fireSelectionCallback(MenuPath::SOUND_ON, 0);

    TEST_ASSERT_TRUE(mockDryer->isSoundEnabled());
}

void test_menu_selection_sound_off() {
    uiController->begin();

    mockDryer->setSoundEnabled(true);

    mockMenu->fireSelectionCallback(MenuPath::SOUND_OFF, 0);

    TEST_ASSERT_FALSE(mockDryer->isSoundEnabled());
}

void test_menu_selection_adjust_timer() {
    uiController->begin();

    // Set initial remaining time and trigger callback so UIController caches it
    CurrentStats stats = mockDryer->getCurrentStats();
    stats.remainingTime = 3600;  // 1 hour
    mockDryer->setStats(stats);
    mockDryer->triggerStatsUpdate();  // Important: UIController caches lastStats from callback

    // Adjust to 2 hours (120 minutes)
    mockMenu->fireSelectionCallback(MenuPath::ADJUST_TIMER, 120);

    // Should have called adjustRemainingTime with delta
    TEST_ASSERT_EQUAL(1, mockDryer->getAdjustRemainingTimeCallCount());

    // Delta should be (120*60 - 3600) = 7200 - 3600 = 3600 seconds
    TEST_ASSERT_EQUAL(3600, mockDryer->getLastAdjustRemainingTimeDelta());
}

void test_menu_selection_back_at_root_exits_menu() {
    uiController->begin();

    // Enter menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);
    TEST_ASSERT_TRUE(uiController->isInMenuMode());

    // Simulate being at ROOT
    mockMenu->setCurrentMenuPath(MenuPath::ROOT);

    // Select BACK
    mockMenu->fireSelectionCallback(MenuPath::BACK, 0);

    TEST_ASSERT_FALSE(uiController->isInMenuMode());
}

// ==================== Display Update Tests ====================

void test_update_calls_button_manager_update() {
    uiController->begin();

    uint32_t initialCount = mockButtons->getUpdateCallCount();

    uiController->update(1000);

    TEST_ASSERT_GREATER_THAN(initialCount, mockButtons->getUpdateCallCount());
}

void test_update_renders_home_screen_initially() {
    uiController->begin();

    mockDisplay->resetCounts();

    uiController->update(1000);

    // Should have called clear and display
    TEST_ASSERT_GREATER_THAN(0, mockDisplay->getClearCallCount());
    TEST_ASSERT_GREATER_THAN(0, mockDisplay->getDisplayCallCount());
}

void test_display_dirty_flag_optimization() {
    uiController->begin();

    // First update renders
    uiController->update(1000);
    mockDisplay->resetCounts();

    // Second update without changes should NOT render
    uiController->update(2000);

    TEST_ASSERT_EQUAL(0, mockDisplay->getClearCallCount());
}

void test_button_press_sets_dirty_flag() {
    uiController->begin();

    uiController->update(1000);
    mockDisplay->resetCounts();

    // Button press should trigger display update
    mockButtons->simulateButtonEvent(ButtonType::UP, ButtonEvent::SINGLE_CLICK);
    uiController->update(2000);

    TEST_ASSERT_GREATER_THAN(0, mockDisplay->getDisplayCallCount());
}

void test_stats_update_in_home_mode_sets_dirty_flag() {
    uiController->begin();

    uiController->update(1000);
    mockDisplay->resetCounts();

    // Update stats with significant change
    CurrentStats newStats = mockDryer->getCurrentStats();
    newStats.boxTemp = 50.0;  // Changed from initial
    mockDryer->setStats(newStats);
    mockDryer->triggerStatsUpdate();

    uiController->update(2000);

    TEST_ASSERT_GREATER_THAN(0, mockDisplay->getDisplayCallCount());
}

void test_stats_update_in_menu_mode_does_not_set_dirty_flag() {
    uiController->begin();

    // Enter menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);
    uiController->update(1000);
    mockDisplay->resetCounts();

    // Update stats
    CurrentStats newStats = mockDryer->getCurrentStats();
    newStats.boxTemp = 50.0;
    mockDryer->setStats(newStats);
    mockDryer->triggerStatsUpdate();

    uiController->update(2000);

    // Should NOT trigger display update
    TEST_ASSERT_EQUAL(0, mockDisplay->getDisplayCallCount());
}

void test_small_stats_changes_dont_trigger_update() {
    uiController->begin();

    uiController->update(1000);
    mockDisplay->resetCounts();

    // Very small change (< 0.05 threshold)
    CurrentStats newStats = mockDryer->getCurrentStats();
    newStats.boxTemp = 0.01;  // Less than 0.05 threshold
    mockDryer->setStats(newStats);
    mockDryer->triggerStatsUpdate();

    uiController->update(2000);

    TEST_ASSERT_EQUAL(0, mockDisplay->getDisplayCallCount());
}

// ==================== Menu Timeout Tests ====================

void test_menu_timeout_exits_to_home() {
    uiController->begin();

    // Enter menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);
    TEST_ASSERT_TRUE(uiController->isInMenuMode());

    // Wait past timeout (30 seconds)
    uiController->update(35000);

    TEST_ASSERT_FALSE(uiController->isInMenuMode());
}

void test_menu_activity_resets_timeout() {
    uiController->begin();

    // Enter menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);

    // Keep activity going
    uiController->update(10000);
    mockButtons->simulateButtonEvent(ButtonType::UP, ButtonEvent::SINGLE_CLICK);

    uiController->update(20000);
    mockButtons->simulateButtonEvent(ButtonType::DOWN, ButtonEvent::SINGLE_CLICK);

    uiController->update(30000);

    // Should still be in menu (timeout reset by button presses)
    TEST_ASSERT_TRUE(uiController->isInMenuMode());
}

// ==================== Sound Tests ====================

void test_button_clicks_play_sound() {
    uiController->begin();

    mockButtons->simulateButtonEvent(ButtonType::UP, ButtonEvent::SINGLE_CLICK);

    TEST_ASSERT_GREATER_THAN(0, mockSound->getClickCount());
}

void test_start_selection_plays_start_sound() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::STATUS_START, 0);

    TEST_ASSERT_GREATER_THAN(0, mockSound->getStartCount());
}

void test_preset_selection_plays_confirm_sound() {
    uiController->begin();

    mockMenu->fireSelectionCallback(MenuPath::PRESET_PLA, 0);

    TEST_ASSERT_GREATER_THAN(0, mockSound->getConfirmCount());
}

// ==================== Integration Tests ====================

void test_full_start_dryer_flow() {
    uiController->begin();
    mockDryer->setState(DryerState::READY);

    // Enter menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);
    TEST_ASSERT_TRUE(uiController->isInMenuMode());

    // Navigate to Status menu (already at ROOT, enter Status)
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);

    // Select START
    mockMenu->fireSelectionCallback(MenuPath::STATUS_START, 0);

    // Should be back at home
    TEST_ASSERT_FALSE(uiController->isInMenuMode());

    // Dryer should be started
    TEST_ASSERT_EQUAL(1, mockDryer->getStartCallCount());
}

void test_full_preset_change_flow() {
    uiController->begin();

    // Enter menu
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);

    // Navigate to Preset menu
    mockButtons->simulateButtonEvent(ButtonType::DOWN, ButtonEvent::SINGLE_CLICK);
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::SINGLE_CLICK);

    // Select PETG
    mockMenu->fireSelectionCallback(MenuPath::PRESET_PETG, 0);

    // Should exit menu
    TEST_ASSERT_FALSE(uiController->isInMenuMode());

    // Preset should be changed
    TEST_ASSERT_EQUAL(PresetType::PETG, mockDryer->getActivePreset());
}

void test_stats_screen_cycling_complete_loop() {
    uiController->begin();

    // Cycle through all 6 screens
    auto screen0 = uiController->getCurrentStatsScreen();

    mockButtons->simulateButtonEvent(ButtonType::UP, ButtonEvent::SINGLE_CLICK);
    auto screen1 = uiController->getCurrentStatsScreen();
    TEST_ASSERT_NOT_EQUAL((int)screen0, (int)screen1);

    mockButtons->simulateButtonEvent(ButtonType::UP, ButtonEvent::SINGLE_CLICK);
    auto screen2 = uiController->getCurrentStatsScreen();

    mockButtons->simulateButtonEvent(ButtonType::UP, ButtonEvent::SINGLE_CLICK);
    auto screen3 = uiController->getCurrentStatsScreen();

    mockButtons->simulateButtonEvent(ButtonType::UP, ButtonEvent::SINGLE_CLICK);
    auto screen4 = uiController->getCurrentStatsScreen();

    mockButtons->simulateButtonEvent(ButtonType::UP, ButtonEvent::SINGLE_CLICK);
    auto screen5 = uiController->getCurrentStatsScreen();

    // One more should wrap back to first
    mockButtons->simulateButtonEvent(ButtonType::UP, ButtonEvent::SINGLE_CLICK);
    auto screenWrapped = uiController->getCurrentStatsScreen();

    TEST_ASSERT_EQUAL((int)screen0, (int)screenWrapped);
}

void test_pause_resume_via_long_press() {
    uiController->begin();

    // Start dryer
    mockDryer->setState(DryerState::READY);
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::LONG_PRESS);
    TEST_ASSERT_EQUAL(1, mockDryer->getStartCallCount());

    // Pause
    mockDryer->setState(DryerState::RUNNING);
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::LONG_PRESS);
    TEST_ASSERT_EQUAL(1, mockDryer->getPauseCallCount());

    // Resume
    mockDryer->setState(DryerState::PAUSED);
    mockButtons->simulateButtonEvent(ButtonType::SET, ButtonEvent::LONG_PRESS);
    TEST_ASSERT_EQUAL(1, mockDryer->getResumeCallCount());
}

// ==================== Main Test Runner ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_begin_initializes_all_components);
    RUN_TEST(test_begin_sets_menu_constraints);
    RUN_TEST(test_begin_initializes_custom_preset);
    RUN_TEST(test_begin_sets_pid_profile);
    RUN_TEST(test_begin_starts_in_home_mode);

    // Button callbacks
    RUN_TEST(test_set_button_single_click_enters_menu_from_home);
    RUN_TEST(test_set_button_long_press_toggles_dryer_from_home);
    RUN_TEST(test_set_button_long_press_pauses_when_running);
    RUN_TEST(test_set_button_long_press_resumes_when_paused);
    RUN_TEST(test_set_button_in_menu_sends_action);
    RUN_TEST(test_set_button_long_press_in_menu_exits_at_root);
    RUN_TEST(test_set_button_long_press_in_submenu_sends_action);
    RUN_TEST(test_up_button_cycles_stats_screen_forward);
    RUN_TEST(test_down_button_cycles_stats_screen_backward);
    RUN_TEST(test_up_button_in_menu_sends_action);
    RUN_TEST(test_down_button_in_menu_sends_action);

    // Menu selections
    RUN_TEST(test_menu_selection_start_calls_dryer_start);
    RUN_TEST(test_menu_selection_start_exits_menu);
    RUN_TEST(test_menu_selection_pause_calls_dryer_pause);
    RUN_TEST(test_menu_selection_reset_calls_dryer_reset);
    RUN_TEST(test_menu_selection_preset_pla);
    RUN_TEST(test_menu_selection_preset_petg);
    RUN_TEST(test_menu_selection_preset_custom);
    RUN_TEST(test_menu_selection_custom_temp);
    RUN_TEST(test_menu_selection_custom_time);
    RUN_TEST(test_menu_selection_custom_overshoot);
    RUN_TEST(test_menu_selection_pid_soft);
    RUN_TEST(test_menu_selection_pid_normal);
    RUN_TEST(test_menu_selection_pid_strong);
    RUN_TEST(test_menu_selection_sound_on);
    RUN_TEST(test_menu_selection_sound_off);
    RUN_TEST(test_menu_selection_adjust_timer);
    RUN_TEST(test_menu_selection_back_at_root_exits_menu);

    // Display updates
    RUN_TEST(test_update_calls_button_manager_update);
    RUN_TEST(test_update_renders_home_screen_initially);
    RUN_TEST(test_display_dirty_flag_optimization);
    RUN_TEST(test_button_press_sets_dirty_flag);
    RUN_TEST(test_stats_update_in_home_mode_sets_dirty_flag);
    RUN_TEST(test_stats_update_in_menu_mode_does_not_set_dirty_flag);
    RUN_TEST(test_small_stats_changes_dont_trigger_update);

    // Menu timeout
    RUN_TEST(test_menu_timeout_exits_to_home);
    RUN_TEST(test_menu_activity_resets_timeout);

    // Sound
    RUN_TEST(test_button_clicks_play_sound);
    RUN_TEST(test_start_selection_plays_start_sound);
    RUN_TEST(test_preset_selection_plays_confirm_sound);

    // Integration
    RUN_TEST(test_full_start_dryer_flow);
    RUN_TEST(test_full_preset_change_flow);
    RUN_TEST(test_stats_screen_cycling_complete_loop);
    RUN_TEST(test_pause_resume_via_long_press);

    return UNITY_END();
}
