#ifdef UNIT_TEST
    #include <unity.h>
#else
    #include <unity.h>
    #include <Arduino.h>
#endif

#include "../../src/control/HeaterControl.h"

// Test fixture
HeaterControl* heater;

void setUp(void) {
    heater = new HeaterControl();
}

void tearDown(void) {
    delete heater;
}

// ==================== Initialization Tests ====================

void test_heater_control_initializes() {
    heater->begin(0);
    TEST_ASSERT_FALSE(heater->isRunning());
    TEST_ASSERT_EQUAL(0, heater->getCurrentPWM());
}

void test_heater_control_starts_not_running() {
    TEST_ASSERT_FALSE(heater->isRunning());
}

// ==================== Start/Stop Tests ====================

void test_heater_control_starts() {
    heater->begin(0);
    heater->start(0);

    TEST_ASSERT_TRUE(heater->isRunning());
}

void test_heater_control_stops() {
    heater->begin(0);
    heater->start(0);
    heater->stop(1000);

    TEST_ASSERT_FALSE(heater->isRunning());
    TEST_ASSERT_EQUAL(0, heater->getCurrentPWM());
}

void test_heater_control_stop_clears_pwm() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(150);

    heater->stop(1000);

    TEST_ASSERT_EQUAL(0, heater->getCurrentPWM());
}

void test_heater_control_emergency_stop() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(200);

    heater->emergencyStop();

    TEST_ASSERT_FALSE(heater->isRunning());
    TEST_ASSERT_EQUAL(0, heater->getCurrentPWM());
}

void test_heater_control_emergency_stop_clears_state() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(200);
    heater->update(1000);

    heater->emergencyStop();

    // Verify pin state is LOW (false)
    TEST_ASSERT_FALSE(heater->getPinState());
}

// ==================== PWM Setting Tests ====================

void test_heater_control_sets_pwm_when_running() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(98);

    TEST_ASSERT_EQUAL(98, heater->getCurrentPWM());
}

void test_heater_control_ignores_pwm_when_not_running() {
    heater->begin(0);
    heater->setPWM(100);

    TEST_ASSERT_EQUAL(0, heater->getCurrentPWM());
}

void test_heater_control_clamps_pwm_to_max() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(PWM_MAX+1); // Above max (100)

    TEST_ASSERT_EQUAL(PWM_MAX, heater->getCurrentPWM());
}

void test_heater_control_clamps_pwm_to_min() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(0); // Below min (0)

    TEST_ASSERT_EQUAL(0, heater->getCurrentPWM());
}

void test_heater_control_accepts_zero_pwm() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(0);

    TEST_ASSERT_EQUAL(0, heater->getCurrentPWM());
}

void test_heater_control_accepts_max_pwm() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(PWM_MAX); // Max (100)

    TEST_ASSERT_EQUAL(PWM_MAX, heater->getCurrentPWM());
}

// ==================== Software PWM Timing Tests ====================

void test_heater_control_pwm_starts_high_when_duty_above_zero() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(50); // 50% duty cycle

    // Update at start of cycle
    heater->update(0);

    // Pin should be HIGH at start of ON period
    TEST_ASSERT_TRUE(heater->getPinState());
}

void test_heater_control_pwm_stays_high_during_on_period() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(50); // 50% duty cycle, ON for 2500ms

    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(1000);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(2000);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(2499);
    TEST_ASSERT_TRUE(heater->getPinState());
}

void test_heater_control_pwm_goes_low_after_on_period() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(50); // 50% duty cycle, ON for 2500ms

    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    // After ON period ends
    heater->update(2500);
    TEST_ASSERT_FALSE(heater->getPinState());

    heater->update(3000);
    TEST_ASSERT_FALSE(heater->getPinState());
}

void test_heater_control_pwm_cycle_repeats() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(50); // 50% duty cycle

    // First cycle
    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(2500);
    TEST_ASSERT_FALSE(heater->getPinState());

    // Second cycle (at 5000ms)
    heater->update(5000);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(7500);
    TEST_ASSERT_FALSE(heater->getPinState());

    // Third cycle (at 10000ms)
    heater->update(10000);
    TEST_ASSERT_TRUE(heater->getPinState());
}

void test_heater_control_pwm_25_percent_duty() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(25); // 25% duty cycle, ON for 1250ms

    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(1000);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(1250);
    TEST_ASSERT_FALSE(heater->getPinState());

    heater->update(2000);
    TEST_ASSERT_FALSE(heater->getPinState());

    heater->update(5000); // New cycle
    TEST_ASSERT_TRUE(heater->getPinState());
}

void test_heater_control_pwm_75_percent_duty() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(75); // 75% duty cycle, ON for 3750ms

    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(3000);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(3750);
    TEST_ASSERT_FALSE(heater->getPinState());

    heater->update(4000);
    TEST_ASSERT_FALSE(heater->getPinState());
}

void test_heater_control_pwm_100_percent_duty() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(100); // 100% duty cycle, always ON

    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(2500);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(4999);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(5000); // New cycle
    TEST_ASSERT_TRUE(heater->getPinState());
}

void test_heater_control_pwm_0_percent_duty() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(0); // 0% duty cycle, always OFF

    heater->update(0);
    TEST_ASSERT_FALSE(heater->getPinState());

    heater->update(2500);
    TEST_ASSERT_FALSE(heater->getPinState());

    heater->update(5000);
    TEST_ASSERT_FALSE(heater->getPinState());
}

void test_heater_control_pwm_very_low_duty() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(1); // 1% duty cycle, ON for ~50ms

    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(49);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(51);
    TEST_ASSERT_FALSE(heater->getPinState());

    heater->update(100);
    TEST_ASSERT_FALSE(heater->getPinState());
}

void test_heater_control_pwm_very_high_duty() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(99); // ~99% duty cycle, ON for ~4950ms

    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(4940);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(4960);
    TEST_ASSERT_FALSE(heater->getPinState());

    heater->update(4999);
    TEST_ASSERT_FALSE(heater->getPinState());
}

// ==================== PWM Update Mid-Cycle Tests ====================

void test_heater_control_pwm_change_mid_cycle() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(50); // 50% duty

    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(1000);
    TEST_ASSERT_TRUE(heater->getPinState());

    // Change PWM mid-cycle
    heater->setPWM(25); // 25% duty

    // Next cycle should use new duty cycle
    heater->update(5000);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(6255); // 25% of 5000ms = 1250ms
    TEST_ASSERT_FALSE(heater->getPinState());
}

void test_heater_control_pwm_increase_mid_cycle() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(25); // 25% duty

    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    // Change to higher duty mid-cycle
    heater->setPWM(75); // 75% duty

    // Next cycle should use new duty cycle
    heater->update(5000);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(8755); // 75% of 5000ms = 3750ms
    TEST_ASSERT_FALSE(heater->getPinState());
}

// ==================== Stop/Start Cycle Tests ====================

void test_heater_control_restart_resets_cycle() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(50);

    heater->update(2000); // Mid-cycle
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->stop(2500); // Stop sets pwm to 0

    heater->start(3000); // Restart at 3000ms
    heater->setPWM(50);

    heater->update(3000);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(5550); // 2500ms into new cycle
    TEST_ASSERT_FALSE(heater->getPinState());
}

void test_heater_control_update_when_not_running() {
    heater->begin(0);
    heater->setPWM(200);

    // Update should not change state
    heater->update(0);
    heater->update(1000);
    heater->update(5000);

    TEST_ASSERT_FALSE(heater->getPinState());
    TEST_ASSERT_EQUAL(0, heater->getCurrentPWM());
}

// ==================== Edge Case Tests ====================

void test_heater_control_rapid_updates() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(50);

    // Rapid updates every 10ms
    for (uint32_t t = 0; t < 5000; t += 10) {
        heater->update(t);
        // Serial.print("t="); Serial.print(t); Serial.print("ms, PinState="); Serial.println(heater->getPinState());
        if (t < 2500) {
            TEST_ASSERT_TRUE(heater->getPinState());
        } else {
            TEST_ASSERT_FALSE(heater->getPinState());
        }
    }
}

void test_heater_control_sparse_updates() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(50); // 50% duty

    // Very sparse updates
    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(3000); // Skip past ON period
    TEST_ASSERT_FALSE(heater->getPinState());

    heater->update(6000); // Into next cycle
    TEST_ASSERT_TRUE(heater->getPinState());
}

void test_heater_control_update_with_same_timestamp() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(128);

    heater->update(1000);
    bool state1 = heater->getPinState();

    // Update with same timestamp
    heater->update(1000);
    bool state2 = heater->getPinState();

    // State should not change
    TEST_ASSERT_EQUAL(state1, state2);
}

void test_heater_control_handles_timestamp_overflow() {
    heater->begin(0);
    heater->start(UINT32_MAX - 1000); // Start near overflow
    heater->setPWM(128);

    heater->update(UINT32_MAX - 1000);
    TEST_ASSERT_TRUE(heater->getPinState());

    // Overflow occurs here
    heater->update(500); // After overflow (assuming millis() wraps)

    // Should handle gracefully and start new cycle
    TEST_ASSERT_TRUE(heater->getPinState());
}

// ==================== Integration Scenario Tests ====================

void test_heater_control_typical_heating_sequence() {
    heater->begin(0);
    heater->start(0);

    // Start with low power
    heater->setPWM(50); // ~20%

    for (uint32_t t = 0; t <= 5000; t += 500) {
        heater->update(t);
    }

    // Increase power
    heater->setPWM(150); // ~60%

    for (uint32_t t = 5000; t <= 10000; t += 500) {
        heater->update(t);
    }

    // Full power
    heater->setPWM(255); // 100%

    for (uint32_t t = 10000; t <= 15000; t += 500) {
        heater->update(t);
        TEST_ASSERT_TRUE(heater->getPinState());
    }

    TEST_ASSERT_TRUE(heater->isRunning());
}

void test_heater_control_emergency_stop_during_heating() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(200);

    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    heater->update(1000);
    TEST_ASSERT_TRUE(heater->isRunning());

    // Emergency stop
    heater->emergencyStop();

    heater->update(2000);
    TEST_ASSERT_FALSE(heater->isRunning());
    TEST_ASSERT_FALSE(heater->getPinState());
    TEST_ASSERT_EQUAL(0, heater->getCurrentPWM());
}

void test_heater_control_multiple_start_stop_cycles() {
    heater->begin(0);

    // Cycle 1
    heater->start(0);
    heater->setPWM(100);
    heater->update(1000);
    heater->stop(2000);
    TEST_ASSERT_FALSE(heater->isRunning());

    // Cycle 2
    heater->start(3000);
    heater->setPWM(150);
    heater->update(4000);
    heater->stop(5000);
    TEST_ASSERT_FALSE(heater->isRunning());

    // Cycle 3
    heater->start(6000);
    heater->setPWM(200);
    heater->update(7000);
    TEST_ASSERT_TRUE(heater->isRunning());
}

void test_heater_control_pwm_ramp_up() {
    heater->begin(0);
    heater->start(0);

    // Ramp up from 0 to 255 over 10 cycles
    for (uint8_t pwm = 0; pwm <= 100; pwm += 10) {
        heater->setPWM(pwm);

        uint32_t cycleStart = (pwm / 10) * 5000;
        heater->update(cycleStart);
        heater->update(cycleStart + 2500);

        TEST_ASSERT_EQUAL(pwm, heater->getCurrentPWM());
    }
}

void test_heater_control_pwm_ramp_down() {
    heater->begin(0);
    heater->start(0);

    // Ramp down from 255 to 0 over 10 cycles
    for (int16_t pwm = 100; pwm >= 0; pwm -= 10) {
        heater->setPWM(pwm);

        uint32_t cycleStart = ((100 - pwm) / 10) * 5000;
        heater->update(cycleStart);
        heater->update(cycleStart + 2500);

        TEST_ASSERT_EQUAL(pwm, heater->getCurrentPWM());
    }
}

// ==================== State Verification Tests ====================

void test_heater_control_pin_state_matches_running_state() {
    heater->begin(0);

    // Not running - pin should be LOW
    heater->update(0);
    TEST_ASSERT_FALSE(heater->getPinState());

    // Start - pin should be HIGH (if PWM > 0)
    heater->start(0);
    heater->setPWM(100);
    heater->update(0);
    TEST_ASSERT_TRUE(heater->getPinState());

    // Stop - pin should be LOW
    heater->stop(1000);
    heater->update(1000);
    TEST_ASSERT_FALSE(heater->getPinState());
}

void test_heater_control_maintains_state_across_updates() {
    heater->begin(0);
    heater->start(0);
    heater->setPWM(128);

    heater->update(0);
    uint8_t pwm1 = heater->getCurrentPWM();
    bool running1 = heater->isRunning();

    heater->update(1000);
    uint8_t pwm2 = heater->getCurrentPWM();
    bool running2 = heater->isRunning();

    TEST_ASSERT_EQUAL(pwm1, pwm2);
    TEST_ASSERT_EQUAL(running1, running2);
}

// ==================== Main Test Runner ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_heater_control_initializes);
    RUN_TEST(test_heater_control_starts_not_running);

    // Start/Stop
    RUN_TEST(test_heater_control_starts);
    RUN_TEST(test_heater_control_stops);
    RUN_TEST(test_heater_control_stop_clears_pwm);
    RUN_TEST(test_heater_control_emergency_stop);
    RUN_TEST(test_heater_control_emergency_stop_clears_state);

    // PWM Setting
    RUN_TEST(test_heater_control_sets_pwm_when_running);
    RUN_TEST(test_heater_control_ignores_pwm_when_not_running);
    RUN_TEST(test_heater_control_clamps_pwm_to_max);
    RUN_TEST(test_heater_control_clamps_pwm_to_min);
    RUN_TEST(test_heater_control_accepts_zero_pwm);
    RUN_TEST(test_heater_control_accepts_max_pwm);

    // Software PWM Timing
    RUN_TEST(test_heater_control_pwm_starts_high_when_duty_above_zero);
    RUN_TEST(test_heater_control_pwm_stays_high_during_on_period);
    RUN_TEST(test_heater_control_pwm_goes_low_after_on_period);
    RUN_TEST(test_heater_control_pwm_cycle_repeats);
    RUN_TEST(test_heater_control_pwm_25_percent_duty);
    RUN_TEST(test_heater_control_pwm_75_percent_duty);
    RUN_TEST(test_heater_control_pwm_100_percent_duty);
    RUN_TEST(test_heater_control_pwm_0_percent_duty);
    RUN_TEST(test_heater_control_pwm_very_low_duty);
    RUN_TEST(test_heater_control_pwm_very_high_duty);

    // PWM Mid-Cycle Updates
    RUN_TEST(test_heater_control_pwm_change_mid_cycle);
    RUN_TEST(test_heater_control_pwm_increase_mid_cycle);

    // Stop/Start Cycles
    RUN_TEST(test_heater_control_restart_resets_cycle);
    RUN_TEST(test_heater_control_update_when_not_running);

    // Edge Cases
    RUN_TEST(test_heater_control_rapid_updates);
    RUN_TEST(test_heater_control_sparse_updates);
    RUN_TEST(test_heater_control_update_with_same_timestamp);
    RUN_TEST(test_heater_control_handles_timestamp_overflow);

    // Integration Scenarios
    RUN_TEST(test_heater_control_typical_heating_sequence);
    RUN_TEST(test_heater_control_emergency_stop_during_heating);
    RUN_TEST(test_heater_control_multiple_start_stop_cycles);
    RUN_TEST(test_heater_control_pwm_ramp_up);
    RUN_TEST(test_heater_control_pwm_ramp_down);

    // State Verification
    RUN_TEST(test_heater_control_pin_state_matches_running_state);
    RUN_TEST(test_heater_control_maintains_state_across_updates);

    return UNITY_END();
}