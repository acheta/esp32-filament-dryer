#ifndef MOCK_FAN_CONTROL_H
#define MOCK_FAN_CONTROL_H

#include "../../src/interfaces/IFanControl.h"

/**
 * MockFanControl - Test double for IFanControl
 *
 * Allows manual control and verification of fan state for testing.
 */
class MockFanControl : public IFanControl {
private:
    bool running;
    uint32_t startCallCount;
    uint32_t stopCallCount;

public:
    MockFanControl()
        : running(false),
          startCallCount(0),
          stopCallCount(0) {
    }

    void start() override {
        startCallCount++;
        running = true;
    }

    void stop() override {
        stopCallCount++;
        running = false;
    }

    bool isRunning() const override {
        return running;
    }

    // ==================== Test Helper Methods ====================

    uint32_t getStartCallCount() const {
        return startCallCount;
    }

    uint32_t getStopCallCount() const {
        return stopCallCount;
    }

    void resetCounts() {
        startCallCount = 0;
        stopCallCount = 0;
    }

    void setRunning(bool state) {
        running = state;
    }
};

#endif