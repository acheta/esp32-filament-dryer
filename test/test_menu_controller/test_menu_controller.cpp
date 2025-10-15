#ifdef UNIT_TEST
    #include <unity.h>
#else
    #include <unity.h>
    #include <Arduino.h>
#endif

#include "../../src/userInterface/MenuController.h"

// Test fixture
MenuController* menu;

// Callback tracking
struct SelectionRecord {
    MenuPath path;
    int value;
};

std::vector<SelectionRecord> selectionHistory;

void selectionCallback(MenuPath path, int value) {
    selectionHistory.push_back({path, value});
}

void setUp(void) {
    menu = new MenuController();
    selectionHistory.clear();
}

void tearDown(void) {
    delete menu;
}

// ==================== Initialization Tests ====================

void test_menu_starts_at_root() {
    TEST_ASSERT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());
}

void test_menu_starts_with_selection_zero() {
    TEST_ASSERT_EQUAL(0, menu->getCurrentSelection());
}

void test_menu_starts_not_in_edit_mode() {
    TEST_ASSERT_FALSE(menu->isInEditMode());
}

void test_menu_returns_root_items() {
    std::vector<MenuItem> items = menu->getCurrentMenuItems();
    TEST_ASSERT_TRUE(items.size() > 0);
}

// ==================== Navigation Tests ====================

void test_navigate_down_increments_selection() {
    int initialSelection = menu->getCurrentSelection();
    menu->handleAction(MenuAction::DOWN);
    TEST_ASSERT_EQUAL(initialSelection + 1, menu->getCurrentSelection());
}

void test_navigate_up_decrements_selection() {
    // First go down a few times
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);

    int beforeUp = menu->getCurrentSelection();
    menu->handleAction(MenuAction::UP);
    TEST_ASSERT_EQUAL(beforeUp - 1, menu->getCurrentSelection());
}

void test_navigate_down_wraps_at_end() {
    std::vector<MenuItem> items = menu->getCurrentMenuItems();
    int itemCount = items.size();

    // Navigate past the end
    for (int i = 0; i < itemCount; i++) {
        menu->handleAction(MenuAction::DOWN);
    }

    // Should wrap to 0
    TEST_ASSERT_EQUAL(0, menu->getCurrentSelection());
}

void test_navigate_up_wraps_at_start() {
    // At position 0, navigate up should wrap to last item
    std::vector<MenuItem> items = menu->getCurrentMenuItems();
    int itemCount = items.size();

    menu->handleAction(MenuAction::UP);
    TEST_ASSERT_EQUAL(itemCount - 1, menu->getCurrentSelection());
}

// ==================== Submenu Navigation Tests ====================

void test_enter_submenu_changes_path() {
    // Navigate to first submenu item and enter
    menu->handleAction(MenuAction::ENTER);

    // Should no longer be at ROOT
    TEST_ASSERT_NOT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());
}

void test_enter_submenu_resets_selection() {
    // Move selection down
    menu->handleAction(MenuAction::DOWN);
    TEST_ASSERT_NOT_EQUAL(0, menu->getCurrentSelection());

    // Enter submenu
    menu->handleAction(MenuAction::ENTER);

    // Selection should reset to 0
    TEST_ASSERT_EQUAL(0, menu->getCurrentSelection());
}

void test_back_returns_to_root() {
    // Enter a submenu
    menu->handleAction(MenuAction::ENTER);
    TEST_ASSERT_NOT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());

    // Go back
    menu->handleAction(MenuAction::BACK);
    TEST_ASSERT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());
}

void test_reset_returns_to_root() {
    // Navigate somewhere
    menu->handleAction(MenuAction::ENTER);
    menu->handleAction(MenuAction::DOWN);

    // Reset
    menu->reset();

    TEST_ASSERT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());
    TEST_ASSERT_EQUAL(0, menu->getCurrentSelection());
    TEST_ASSERT_FALSE(menu->isInEditMode());
}

// ==================== Callback Tests ====================

void test_register_callback() {
    menu->registerSelectionCallback(selectionCallback);
    // Should not crash
    TEST_PASS();
}

void test_callback_fires_on_action_selection() {
    menu->registerSelectionCallback(selectionCallback);

    // Navigate to "Status" submenu and enter
    menu->handleAction(MenuAction::ENTER);

    // Navigate to an action item (like "Start/Resume") and select
    menu->handleAction(MenuAction::ENTER);

    // Callback should have been fired
    TEST_ASSERT_GREATER_THAN(0, selectionHistory.size());
}

// ==================== Edit Mode Tests ====================

void test_enter_edit_mode_on_value_item() {
    // Navigate to "Adjust Timer" (which is a VALUE_EDIT item)
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);

    // Enter edit mode
    menu->handleAction(MenuAction::ENTER);

    // Should be in edit mode
    TEST_ASSERT_TRUE(menu->isInEditMode());
}

void test_edit_value_with_up() {
    menu->registerSelectionCallback(selectionCallback);

    // Navigate to "Adjust Timer"
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::ENTER);

    int initialValue = menu->getEditValue();
    menu->handleAction(MenuAction::UP);

    // Value should have increased
    TEST_ASSERT_GREATER_THAN(initialValue, menu->getEditValue());
}

void test_edit_value_with_down() {
    menu->registerSelectionCallback(selectionCallback);

    // Navigate to "Adjust Timer"
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::ENTER);

    // Increase value first
    menu->handleAction(MenuAction::UP);
    menu->handleAction(MenuAction::UP);

    int afterIncrease = menu->getEditValue();
    menu->handleAction(MenuAction::DOWN);

    // Value should have decreased
    TEST_ASSERT_LESS_THAN(afterIncrease, menu->getEditValue());
}

void test_confirm_edit_exits_edit_mode() {
    menu->registerSelectionCallback(selectionCallback);

    // Enter edit mode for "Adjust Timer"
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::ENTER);

    TEST_ASSERT_TRUE(menu->isInEditMode());

    // Confirm with ENTER
    menu->handleAction(MenuAction::ENTER);

    TEST_ASSERT_FALSE(menu->isInEditMode());
}

void test_confirm_edit_fires_callback() {
    menu->registerSelectionCallback(selectionCallback);
    selectionHistory.clear();

    // Enter edit mode for "Adjust Timer"
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::ENTER);

    // Change value
    menu->handleAction(MenuAction::UP);

    // Confirm
    menu->handleAction(MenuAction::ENTER);

    // Callback should have been fired with ADJUST_TIMER path
    TEST_ASSERT_GREATER_THAN(0, selectionHistory.size());
    TEST_ASSERT_EQUAL(MenuPath::ADJUST_TIMER, selectionHistory.back().path);
}

void test_cancel_edit_exits_edit_mode() {
    // Enter edit mode
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::ENTER);

    TEST_ASSERT_TRUE(menu->isInEditMode());

    // Cancel with BACK
    menu->handleAction(MenuAction::BACK);

    TEST_ASSERT_FALSE(menu->isInEditMode());
}

// ==================== Constraints Tests ====================

void test_set_constraints() {
    menu->setConstraints(30.0, 80.0, 36000, 10.0);
    // Should not crash
    TEST_PASS();
}

void test_set_custom_preset_values() {
    menu->setCustomPresetValues(55.0, 18000, 8.0);
    // Should not crash
    TEST_PASS();
}

void test_set_pid_profile() {
    menu->setPIDProfile("STRONG");
    // Should not crash
    TEST_PASS();
}

void test_set_sound_enabled() {
    menu->setSoundEnabled(false);
    menu->setSoundEnabled(true);
    TEST_PASS();
}

void test_set_remaining_time() {
    menu->setRemainingTime(7200);
    TEST_PASS();
}

// ==================== Menu Item Generation Tests ====================

void test_root_menu_has_expected_items() {
    std::vector<MenuItem> items = menu->getCurrentMenuItems();

    // Root menu should have: Status, Select Preset, Edit Custom, Adjust Timer, PID, Sound, System Info, Back
    TEST_ASSERT_GREATER_OR_EQUAL(7, items.size());
}

void test_status_menu_accessible() {
    // Enter Status submenu
    menu->handleAction(MenuAction::ENTER);

    std::vector<MenuItem> items = menu->getCurrentMenuItems();
    TEST_ASSERT_GREATER_THAN(0, items.size());
}

void test_preset_menu_accessible() {
    // Navigate to "Select Preset" and enter
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::ENTER);

    std::vector<MenuItem> items = menu->getCurrentMenuItems();
    // Should have PLA, PETG, Custom, Back
    TEST_ASSERT_GREATER_OR_EQUAL(3, items.size());
}

void test_custom_preset_menu_accessible() {
    // Navigate to "Edit Custom" and enter
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::ENTER);

    std::vector<MenuItem> items = menu->getCurrentMenuItems();
    // Should have Temp, Time, Overshoot, Copy from PLA, Back
    TEST_ASSERT_GREATER_OR_EQUAL(4, items.size());
}

void test_pid_menu_accessible() {
    // Navigate to PID menu
    for (int i = 0; i < 4; i++) {
        menu->handleAction(MenuAction::DOWN);
    }
    menu->handleAction(MenuAction::ENTER);

    std::vector<MenuItem> items = menu->getCurrentMenuItems();
    // Should have SOFT, NORMAL, STRONG, Back
    TEST_ASSERT_GREATER_OR_EQUAL(3, items.size());
}

// ==================== Edge Cases ====================

void test_multiple_resets() {
    menu->reset();
    menu->reset();
    menu->reset();

    TEST_ASSERT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());
    TEST_ASSERT_EQUAL(0, menu->getCurrentSelection());
}

void test_rapid_navigation() {
    // Rapid up/down navigation
    for (int i = 0; i < 100; i++) {
        menu->handleAction(MenuAction::DOWN);
    }
    for (int i = 0; i < 100; i++) {
        menu->handleAction(MenuAction::UP);
    }

    // Should still be functional
    TEST_ASSERT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());
}

void test_back_at_root_stays_at_root() {
    TEST_ASSERT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());

    menu->handleAction(MenuAction::BACK);

    // Should still be at root
    TEST_ASSERT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());
}

void test_get_edit_item_when_not_editing() {
    TEST_ASSERT_FALSE(menu->isInEditMode());

    // Should not crash when calling getEditingItem
    MenuItem item = menu->getEditingItem();
    TEST_PASS();
}

void test_get_edit_value_when_not_editing() {
    TEST_ASSERT_FALSE(menu->isInEditMode());

    // Should return a valid value (default is 240 = 4 hours in 10min increments)
    int value = menu->getEditValue();
    TEST_ASSERT_EQUAL(240, value);
}

// ==================== Integration Tests ====================

void test_full_navigation_flow() {
    // Start at root
    TEST_ASSERT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());

    // Enter Status
    menu->handleAction(MenuAction::ENTER);
    TEST_ASSERT_EQUAL(MenuPath::STATUS, menu->getCurrentMenuPath());

    // Go back
    menu->handleAction(MenuAction::BACK);
    TEST_ASSERT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());

    // Navigate to Preset
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::ENTER);
    TEST_ASSERT_EQUAL(MenuPath::PRESET, menu->getCurrentMenuPath());

    // Back to root
    menu->handleAction(MenuAction::BACK);
    TEST_ASSERT_EQUAL(MenuPath::ROOT, menu->getCurrentMenuPath());
}

void test_edit_flow_complete() {
    menu->registerSelectionCallback(selectionCallback);
    selectionHistory.clear();

    // Navigate to Adjust Timer
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);
    menu->handleAction(MenuAction::DOWN);

    // Enter edit mode
    menu->handleAction(MenuAction::ENTER);
    TEST_ASSERT_TRUE(menu->isInEditMode());

    // Modify value
    int initial = menu->getEditValue();
    menu->handleAction(MenuAction::UP);
    menu->handleAction(MenuAction::UP);
    TEST_ASSERT_GREATER_THAN(initial, menu->getEditValue());

    // Confirm
    menu->handleAction(MenuAction::ENTER);
    TEST_ASSERT_FALSE(menu->isInEditMode());

    // Callback should have fired
    TEST_ASSERT_GREATER_THAN(0, selectionHistory.size());
}

// ==================== Main Test Runner ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_menu_starts_at_root);
    RUN_TEST(test_menu_starts_with_selection_zero);
    RUN_TEST(test_menu_starts_not_in_edit_mode);
    RUN_TEST(test_menu_returns_root_items);

    // Navigation
    RUN_TEST(test_navigate_down_increments_selection);
    RUN_TEST(test_navigate_up_decrements_selection);
    RUN_TEST(test_navigate_down_wraps_at_end);
    RUN_TEST(test_navigate_up_wraps_at_start);

    // Submenu navigation
    RUN_TEST(test_enter_submenu_changes_path);
    RUN_TEST(test_enter_submenu_resets_selection);
    RUN_TEST(test_back_returns_to_root);
    RUN_TEST(test_reset_returns_to_root);

    // Callbacks
    RUN_TEST(test_register_callback);
    RUN_TEST(test_callback_fires_on_action_selection);

    // Edit mode
    RUN_TEST(test_enter_edit_mode_on_value_item);
    RUN_TEST(test_edit_value_with_up);
    RUN_TEST(test_edit_value_with_down);
    RUN_TEST(test_confirm_edit_exits_edit_mode);
    RUN_TEST(test_confirm_edit_fires_callback);
    RUN_TEST(test_cancel_edit_exits_edit_mode);

    // Constraints
    RUN_TEST(test_set_constraints);
    RUN_TEST(test_set_custom_preset_values);
    RUN_TEST(test_set_pid_profile);
    RUN_TEST(test_set_sound_enabled);
    RUN_TEST(test_set_remaining_time);

    // Menu items
    RUN_TEST(test_root_menu_has_expected_items);
    RUN_TEST(test_status_menu_accessible);
    RUN_TEST(test_preset_menu_accessible);
    RUN_TEST(test_custom_preset_menu_accessible);
    RUN_TEST(test_pid_menu_accessible);

    // Edge cases
    RUN_TEST(test_multiple_resets);
    RUN_TEST(test_rapid_navigation);
    RUN_TEST(test_back_at_root_stays_at_root);
    RUN_TEST(test_get_edit_item_when_not_editing);
    RUN_TEST(test_get_edit_value_when_not_editing);

    // Integration
    RUN_TEST(test_full_navigation_flow);
    RUN_TEST(test_edit_flow_complete);

    return UNITY_END();
}
