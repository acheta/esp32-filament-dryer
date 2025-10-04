#ifndef MOCK_DISPLAY_H
#define MOCK_DISPLAY_H

#include "../../src/interfaces/IDisplay.h"
#include <vector>

/**
 * MockDisplay - Test double for IDisplay
 *
 * Tracks all display operations for verification in tests.
 */
class MockDisplay : public IDisplay {
private:
    bool initialized;
    uint32_t clearCallCount;
    uint32_t displayCallCount;
    uint32_t showSensorReadingsCallCount;

    // Last values shown
    float lastHeaterTemp;
    bool lastHeaterValid;
    float lastBoxTemp;
    float lastBoxHumidity;
    bool lastBoxValid;

    // Text rendering tracking
    struct TextCommand {
        uint8_t x;
        uint8_t y;
        uint8_t size;
        String text;
    };
    std::vector<TextCommand> textCommands;

    uint8_t currentCursorX;
    uint8_t currentCursorY;
    uint8_t currentTextSize;

public:
    MockDisplay()
        : initialized(false),
          clearCallCount(0),
          displayCallCount(0),
          showSensorReadingsCallCount(0),
          lastHeaterTemp(0),
          lastHeaterValid(false),
          lastBoxTemp(0),
          lastBoxHumidity(0),
          lastBoxValid(false),
          currentCursorX(0),
          currentCursorY(0),
          currentTextSize(1) {
    }

    void begin() override {
        initialized = true;
    }

    void clear() override {
        clearCallCount++;
        textCommands.clear();
    }

    void display() override {
        displayCallCount++;
    }

    void showSensorReadings(float heaterTemp, bool heaterValid,
                           float boxTemp, float boxHumidity, bool boxValid) override {
        showSensorReadingsCallCount++;
        lastHeaterTemp = heaterTemp;
        lastHeaterValid = heaterValid;
        lastBoxTemp = boxTemp;
        lastBoxHumidity = boxHumidity;
        lastBoxValid = boxValid;
    }

    void setCursor(uint8_t x, uint8_t y) override {
        currentCursorX = x;
        currentCursorY = y;
    }

    void setTextSize(uint8_t size) override {
        currentTextSize = size;
    }

    void print(const String& text) override {
        TextCommand cmd;
        cmd.x = currentCursorX;
        cmd.y = currentCursorY;
        cmd.size = currentTextSize;
        cmd.text = text;
        textCommands.push_back(cmd);
    }

    void println(const String& text) override {
        print(text);
        currentCursorY += 8 * currentTextSize; // Simulate newline
    }

    // ==================== Test Helper Methods ====================

    bool isInitialized() const { return initialized; }
    uint32_t getClearCallCount() const { return clearCallCount; }
    uint32_t getDisplayCallCount() const { return displayCallCount; }
    uint32_t getShowSensorReadingsCallCount() const { return showSensorReadingsCallCount; }

    float getLastHeaterTemp() const { return lastHeaterTemp; }
    bool getLastHeaterValid() const { return lastHeaterValid; }
    float getLastBoxTemp() const { return lastBoxTemp; }
    float getLastBoxHumidity() const { return lastBoxHumidity; }
    bool getLastBoxValid() const { return lastBoxValid; }

    size_t getTextCommandCount() const { return textCommands.size(); }

    String getTextAtIndex(size_t index) const {
        if (index < textCommands.size()) {
            return textCommands[index].text;
        }
        return "";
    }

    void resetCounts() {
        clearCallCount = 0;
        displayCallCount = 0;
        showSensorReadingsCallCount = 0;
        textCommands.clear();
    }
};

#endif