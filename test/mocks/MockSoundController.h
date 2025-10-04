#ifndef MOCK_SOUND_CONTROLLER_H
#define MOCK_SOUND_CONTROLLER_H

#include "../../src/interfaces/ISoundController.h"
#include <cstdint>

#ifdef UNIT_TEST
    #include "../../test/mocks/arduino_mock.h"
#endif

class MockSoundController : public ISoundController {
private:
    bool initialized;
    bool enabled;
    uint32_t clickCount;
    uint32_t confirmCount;
    uint32_t startCount;
    uint32_t finishedCount;
    uint32_t alarmCount;

public:
    MockSoundController()
        : initialized(false),
          enabled(true),
          clickCount(0),
          confirmCount(0),
          startCount(0),
          finishedCount(0),
          alarmCount(0) {
    }

    void begin() override {
        initialized = true;
    }

    void setEnabled(bool en) override {
        enabled = en;
    }

    bool isEnabled() const override {
        return enabled;
    }

    void playClick() override {
        if (enabled) clickCount++;
    }

    void playConfirm() override {
        if (enabled) confirmCount++;
    }

    void playStart() override {
        if (enabled) startCount++;
    }

    void playFinished() override {
        if (enabled) finishedCount++;
    }

    void playAlarm() override {
        if (enabled) alarmCount++;
    }

    // Test helpers
    bool isInitialized() const { return initialized; }
    uint32_t getClickCount() const { return clickCount; }
    uint32_t getConfirmCount() const { return confirmCount; }
    uint32_t getStartCount() const { return startCount; }
    uint32_t getFinishedCount() const { return finishedCount; }
    uint32_t getAlarmCount() const { return alarmCount; }

    void resetCounts() {
        clickCount = 0;
        confirmCount = 0;
        startCount = 0;
        finishedCount = 0;
        alarmCount = 0;
    }
};

#endif