#ifndef MOCK_MENU_CONTROLLER_H
#define MOCK_MENU_CONTROLLER_H

#include "../../src/interfaces/IMenuController.h"
#include <vector>

/**
 * MockMenuController - Test double for MenuController
 *
 * Allows tests to:
 * - Track menu navigation calls
 * - Simulate menu state
 * - Verify callback registration
 * - Control menu items returned
 */
class MockMenuController : public IMenuController {
private:
    MenuPath currentPath;
    int currentSelection;
    bool inEditMode;
    MenuItem editingItem;
    int editValue;

    std::vector<MenuItem> menuItems;
    std::vector<MenuSelectionCallback> callbacks;

    // Configuration values
    float minTemp;
    float maxTemp;
    uint32_t maxTime;
    float maxOvershoot;
    float customTemp;
    uint32_t customTime;
    float customOvershoot;
    String pidProfile;
    bool soundEnabled;
    uint32_t remainingTime;

    // Call tracking
    int resetCallCount;
    int handleActionCallCount;
    bool setConstraintsCalled;
    bool setCustomPresetValuesCalled;
    bool setPIDProfileCalled;

public:
    MockMenuController()
        : currentPath(MenuPath::ROOT),
          currentSelection(0),
          inEditMode(false),
          editValue(0),
          minTemp(30.0),
          maxTemp(80.0),
          maxTime(36000),
          maxOvershoot(10.0),
          customTemp(50.0),
          customTime(18000),
          customOvershoot(10.0),
          pidProfile("NORMAL"),
          soundEnabled(true),
          remainingTime(0),
          resetCallCount(0),
          handleActionCallCount(0),
          setConstraintsCalled(false),
          setCustomPresetValuesCalled(false),
          setPIDProfileCalled(false) {

        // Default menu items (empty)
        menuItems.clear();
    }

    void handleAction(MenuAction action) override {
        handleActionCallCount++;
        // Simplified behavior - real logic is tested in MenuController tests
    }

    void reset() override {
        resetCallCount++;
        currentPath = MenuPath::ROOT;
        currentSelection = 0;
        inEditMode = false;
    }

    std::vector<MenuItem> getCurrentMenuItems() override {
        return menuItems;
    }

    MenuPath getCurrentMenuPath() const override {
        return currentPath;
    }

    int getCurrentSelection() const override {
        return currentSelection;
    }

    bool isInEditMode() const override {
        return inEditMode;
    }

    MenuItem getEditingItem() const override {
        return editingItem;
    }

    int getEditValue() const override {
        return editValue;
    }

    void setConstraints(float minT, float maxT, uint32_t maxTm, float maxOvr) override {
        minTemp = minT;
        maxTemp = maxT;
        maxTime = maxTm;
        maxOvershoot = maxOvr;
        setConstraintsCalled = true;
    }

    void setCustomPresetValues(float temp, uint32_t time, float overshoot) override {
        customTemp = temp;
        customTime = time;
        customOvershoot = overshoot;
        setCustomPresetValuesCalled = true;
    }

    void setPIDProfile(const String& profile) override {
        pidProfile = profile;
        setPIDProfileCalled = true;
    }

    void setSoundEnabled(bool enabled) override {
        soundEnabled = enabled;
    }

    void setRemainingTime(uint32_t seconds) override {
        remainingTime = seconds;
    }

    void registerSelectionCallback(MenuSelectionCallback callback) override {
        callbacks.push_back(callback);
    }

    // ====================  Test Helper Methods ====================

    /**
     * Set current menu path (for testing)
     */
    void setCurrentMenuPath(MenuPath path) {
        currentPath = path;
    }

    /**
     * Set current selection (for testing)
     */
    void setCurrentSelection(int selection) {
        currentSelection = selection;
    }

    /**
     * Set edit mode state (for testing)
     */
    void setEditMode(bool editing) {
        inEditMode = editing;
    }

    /**
     * Set editing item (for testing)
     */
    void setEditingItem(const MenuItem& item) {
        editingItem = item;
    }

    /**
     * Set edit value (for testing)
     */
    void setEditValueDirect(int value) {
        editValue = value;
    }

    /**
     * Set menu items to return (for testing)
     */
    void setMenuItems(const std::vector<MenuItem>& items) {
        menuItems = items;
    }

    /**
     * Fire selection callback (for testing)
     */
    void fireSelectionCallback(MenuPath path, int value) {
        for (auto& callback : callbacks) {
            if (callback) {
                callback(path, value);
            }
        }
    }

    /**
     * Get number of callbacks registered
     */
    size_t getCallbackCount() const {
        return callbacks.size();
    }

    /**
     * Get reset call count
     */
    int getResetCallCount() const {
        return resetCallCount;
    }

    /**
     * Get handle action call count
     */
    int getHandleActionCallCount() const {
        return handleActionCallCount;
    }

    /**
     * Check if methods were called
     */
    bool wasSetConstraintsCalled() const {
        return setConstraintsCalled;
    }

    bool wasSetCustomPresetValuesCalled() const {
        return setCustomPresetValuesCalled;
    }

    bool wasSetPIDProfileCalled() const {
        return setPIDProfileCalled;
    }

    /**
     * Reset call counts (for testing between different scenarios)
     */
    void resetCallCounts() {
        resetCallCount = 0;
        handleActionCallCount = 0;
        setConstraintsCalled = false;
        setCustomPresetValuesCalled = false;
        setPIDProfileCalled = false;
    }

    /**
     * Get stored configuration values
     */
    float getMinTemp() const { return minTemp; }
    float getMaxTemp() const { return maxTemp; }
    uint32_t getMaxTime() const { return maxTime; }
    float getMaxOvershoot() const { return maxOvershoot; }
    float getCustomTemp() const { return customTemp; }
    uint32_t getCustomTime() const { return customTime; }
    float getCustomOvershoot() const { return customOvershoot; }
    String getPIDProfile() const { return pidProfile; }
    bool getSoundEnabled() const { return soundEnabled; }
    uint32_t getRemainingTime() const { return remainingTime; }

    /**
     * Reset mock state
     */
    void resetMock() {
        currentPath = MenuPath::ROOT;
        currentSelection = 0;
        inEditMode = false;
        editValue = 0;
        menuItems.clear();
        callbacks.clear();
        resetCallCount = 0;
        handleActionCallCount = 0;
    }
};

#endif
