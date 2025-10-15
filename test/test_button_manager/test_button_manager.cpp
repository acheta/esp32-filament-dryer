#ifdef UNIT_TEST
    #include <unity.h>
#else
    #include <unity.h>
    #include <Arduino.h>
#endif

#include "../../src/userInterface/ButtonManager.h"

// Test fixture
ButtonManager* buttonManager;

// Callback tracking
struct CallbackTracker {
    int callCount;
    ButtonType lastButton;
    ButtonEvent lastEvent;

    void reset() {
        callCount = 0;
        lastButton = ButtonType::SET;
        lastEvent = ButtonEvent::SINGLE_CLICK;
    }
};

CallbackTracker setTracker;
CallbackTracker upTracker;
CallbackTracker downTracker;

// Callback functions for testing
void setButtonCallback(ButtonEvent event) {
    setTracker.callCount++;
    setTracker.lastButton = ButtonType::SET;
    setTracker.lastEvent = event;
}

void upButtonCallback(ButtonEvent event) {
    upTracker.callCount++;
    upTracker.lastButton = ButtonType::UP;
    upTracker.lastEvent = event;
}

void downButtonCallback(ButtonEvent event) {
    downTracker.callCount++;
    downTracker.lastButton = ButtonType::DOWN;
    downTracker.lastEvent = event;
}

void setUp(void) {
    buttonManager = new ButtonManager();
    buttonManager->begin();

    setTracker.reset();
    upTracker.reset();
    downTracker.reset();
}

void tearDown(void) {
    delete buttonManager;
}

// ==================== Initialization Tests ====================

void test_button_manager_begin_initializes() {
    // After begin(), buttons should not be pressed
    TEST_ASSERT_FALSE(buttonManager->isButtonPressed(ButtonType::SET));
    TEST_ASSERT_FALSE(buttonManager->isButtonPressed(ButtonType::UP));
    TEST_ASSERT_FALSE(buttonManager->isButtonPressed(ButtonType::DOWN));
}

// ==================== Callback Registration Tests ====================

void test_register_set_button_callback() {
    buttonManager->registerButtonCallback(ButtonType::SET, setButtonCallback);

    // Verify callback can be called (implementation detail - we can't directly test
    // since OneButton is hardware-dependent, but we can verify registration doesn't crash)
    TEST_PASS();
}

void test_register_up_button_callback() {
    buttonManager->registerButtonCallback(ButtonType::UP, upButtonCallback);
    TEST_PASS();
}

void test_register_down_button_callback() {
    buttonManager->registerButtonCallback(ButtonType::DOWN, downButtonCallback);
    TEST_PASS();
}

void test_register_all_button_callbacks() {
    buttonManager->registerButtonCallback(ButtonType::SET, setButtonCallback);
    buttonManager->registerButtonCallback(ButtonType::UP, upButtonCallback);
    buttonManager->registerButtonCallback(ButtonType::DOWN, downButtonCallback);
    TEST_PASS();
}

void test_register_null_callback() {
    // Should handle null callback gracefully
    buttonManager->registerButtonCallback(ButtonType::SET, nullptr);
    TEST_PASS();
}

void test_replace_callback() {
    // Register initial callback
    buttonManager->registerButtonCallback(ButtonType::SET, setButtonCallback);

    // Replace with different callback
    buttonManager->registerButtonCallback(ButtonType::SET, upButtonCallback);

    // Should not crash
    TEST_PASS();
}

// ==================== Update Method Tests ====================

void test_update_method_exists() {
    // update() should not crash
    uint32_t testTime = 1000;
    buttonManager->update(testTime);
    TEST_PASS();
}

void test_update_multiple_times() {
    // Multiple updates should work
    buttonManager->update(0);
    buttonManager->update(100);
    buttonManager->update(200);
    buttonManager->update(500);
    TEST_PASS();
}

// ==================== Button State Query Tests ====================

void test_is_button_pressed_returns_bool() {
    // isButtonPressed should return false for not pressed buttons
    // (in test environment, hardware won't be actually pressed)
    bool setPressed = buttonManager->isButtonPressed(ButtonType::SET);
    bool upPressed = buttonManager->isButtonPressed(ButtonType::UP);
    bool downPressed = buttonManager->isButtonPressed(ButtonType::DOWN);

    // In test environment, all should be false (no physical buttons)
    TEST_ASSERT_FALSE(setPressed);
    TEST_ASSERT_FALSE(upPressed);
    TEST_ASSERT_FALSE(downPressed);
}

// ==================== Integration Tests (Callback System) ====================

void test_callback_system_structure() {
    // Verify the callback system is set up correctly
    // Register all callbacks
    buttonManager->registerButtonCallback(ButtonType::SET, setButtonCallback);
    buttonManager->registerButtonCallback(ButtonType::UP, upButtonCallback);
    buttonManager->registerButtonCallback(ButtonType::DOWN, downButtonCallback);

    // Update to process any button states
    buttonManager->update(0);

    // In test environment (no physical hardware), callbacks won't fire
    // But the system should be ready
    TEST_PASS();
}

// ==================== Edge Case Tests ====================

void test_update_with_zero_time() {
    buttonManager->update(0);
    TEST_PASS();
}

void test_update_with_large_time() {
    buttonManager->update(0xFFFFFFFF);  // Max uint32_t
    TEST_PASS();
}

void test_multiple_begin_calls() {
    buttonManager->begin();
    buttonManager->begin();
    buttonManager->begin();

    // Should handle multiple begin() calls gracefully
    TEST_PASS();
}

void test_update_before_begin() {
    // Create new instance without calling begin
    ButtonManager* testManager = new ButtonManager();

    // update() before begin() should not crash
    testManager->update(0);

    delete testManager;
    TEST_PASS();
}

// ==================== Stress Tests ====================

void test_rapid_updates() {
    // Simulate rapid update calls (like in main loop)
    for (int i = 0; i < 1000; i++) {
        buttonManager->update(i);
    }
    TEST_PASS();
}

void test_register_callbacks_multiple_times() {
    // Register same callback multiple times
    for (int i = 0; i < 10; i++) {
        buttonManager->registerButtonCallback(ButtonType::SET, setButtonCallback);
    }
    TEST_PASS();
}

// ==================== Button Type Coverage Tests ====================

void test_all_button_types_supported() {
    // Verify all button types are handled
    buttonManager->registerButtonCallback(ButtonType::SET, setButtonCallback);
    buttonManager->registerButtonCallback(ButtonType::UP, upButtonCallback);
    buttonManager->registerButtonCallback(ButtonType::DOWN, downButtonCallback);

    TEST_ASSERT_FALSE(buttonManager->isButtonPressed(ButtonType::SET));
    TEST_ASSERT_FALSE(buttonManager->isButtonPressed(ButtonType::UP));
    TEST_ASSERT_FALSE(buttonManager->isButtonPressed(ButtonType::DOWN));
}

// ==================== Main Test Runner ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_button_manager_begin_initializes);

    // Callback registration
    RUN_TEST(test_register_set_button_callback);
    RUN_TEST(test_register_up_button_callback);
    RUN_TEST(test_register_down_button_callback);
    RUN_TEST(test_register_all_button_callbacks);
    RUN_TEST(test_register_null_callback);
    RUN_TEST(test_replace_callback);

    // Update method
    RUN_TEST(test_update_method_exists);
    RUN_TEST(test_update_multiple_times);

    // State queries
    RUN_TEST(test_is_button_pressed_returns_bool);

    // Integration
    RUN_TEST(test_callback_system_structure);

    // Edge cases
    RUN_TEST(test_update_with_zero_time);
    RUN_TEST(test_update_with_large_time);
    RUN_TEST(test_multiple_begin_calls);
    RUN_TEST(test_update_before_begin);

    // Stress tests
    RUN_TEST(test_rapid_updates);
    RUN_TEST(test_register_callbacks_multiple_times);

    // Coverage
    RUN_TEST(test_all_button_types_supported);

    return UNITY_END();
}
