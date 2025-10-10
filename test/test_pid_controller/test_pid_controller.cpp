#ifdef UNIT_TEST
    #include <unity.h>
#else
    #include <unity.h>
    #include <Arduino.h>
#endif

#include "../../src/control/PIDController.h"

PIDController* pid;

void setUp(void) {
    pid = new PIDController();
}

void tearDown(void) {
    delete pid;
}

// ==================== Initialization Tests ====================

void test_pid_initializes() {
    pid->begin();
    // No crash = success
    TEST_ASSERT_TRUE(true);
}

void test_pid_starts_with_zero_output() {
    pid->begin();
    float output = pid->compute(50.0, 25.0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0, output); // First run returns 0
}

// ==================== Profile Tests ====================

void test_pid_accepts_profile_changes() {
    pid->begin();
    pid->setProfile(PIDProfile::SOFT);
    pid->setProfile(PIDProfile::NORMAL);
    pid->setProfile(PIDProfile::STRONG);
    // No crash = success
    TEST_ASSERT_TRUE(true);
}

// ==================== Basic PID Tests ====================

void test_pid_produces_output_after_first_compute() {
    pid->begin();
    pid->setProfile(PIDProfile::NORMAL);
    pid->setLimits(0, 255);

    // First compute (initialization)
    pid->compute(50.0, 25.0, 0);

    // Second compute should produce output
    float output = pid->compute(50.0, 25.0, 1000);

    // Should have positive output (error = 50-25 = 25)
    TEST_ASSERT_TRUE(output > 0);
}

void test_pid_proportional_term_responds_to_error() {
    pid->begin();
    pid->setProfile(PIDProfile::NORMAL);
    pid->setLimits(0, 30);

    pid->compute(50.0, 25.0, 0);

    // Larger error should produce larger output
    float output1 = pid->compute(50.0, 40, 1000); // error = 25
    float output2 = pid->compute(50.0, 30, 2000); // error = 40
//Serial.print("Output1: "); Serial.print((output1));
//Serial.print(", Output2: "); Serial.print((output2));
    TEST_ASSERT_TRUE(output2 > output1);
}

void test_pid_accumulates_integral() {
    pid->begin();
    pid->setProfile(PIDProfile::NORMAL);
    pid->setLimits(0, 255);

    pid->compute(50.0, 45.0, 0); // Initialize

    float output1 = pid->compute(50.0, 45.0, 1000);
    float output2 = pid->compute(50.0, 45.0, 2000);
    float output3 = pid->compute(50.0, 45.0, 3000);

    // Integral should accumulate, increasing output
    TEST_ASSERT_TRUE(output2 > output1);
    TEST_ASSERT_TRUE(output3 > output2);
}

void test_pid_derivative_opposes_change() {
    pid->begin();
    pid->setProfile(PIDProfile::STRONG); // High Kd for visible effect
    pid->setLimits(0, 255);

    pid->compute(50.0, 40.0, 0);

    // Rapid increase should be opposed by derivative
    float outputStatic = pid->compute(50.0, 45.0, 1000);

    pid->reset();
    pid->compute(50.0, 40.0, 0);
    float outputRising = pid->compute(50.0, 49.0, 1000); // Rapid rise

    // Rising temperature should reduce output more due to derivative
    TEST_ASSERT_TRUE(outputStatic > outputRising);
}

// ==================== PWM_MAX_PID_OUTPUT Limit Tests ====================

void test_pid_respects_pwm_max_pid_output_limit() {
    pid->begin();
    pid->setProfile(PIDProfile::STRONG);
    pid->setLimits(0, 255); // Try to set higher limit

    // PID should internally cap to PWM_MAX_PID_OUTPUT (30)
    TEST_ASSERT_EQUAL_FLOAT(PWM_MAX_PID_OUTPUT, pid->getOutputMax());
}

void test_pid_output_never_exceeds_pwm_max_pid_output() {
    pid->begin();
    pid->setProfile(PIDProfile::STRONG);
    pid->setLimits(0, 255); // Try to set higher limit

    pid->compute(50.0, 0.0, 0);

    // Even with huge error, output should be capped at PWM_MAX_PID_OUTPUT
    float output = pid->compute(50.0, 0.0, 1000); // Huge error

    TEST_ASSERT_TRUE(output <= PWM_MAX_PID_OUTPUT);
    TEST_ASSERT_TRUE(output >= 0);
}

void test_pid_integral_windup_limited_to_pwm_max_pid_output() {
    pid->begin();
    pid->setProfile(PIDProfile::NORMAL);
    pid->setLimits(0, 255); // Try to set higher limit

    pid->compute(50.0, 0.0, 0);

    // Let integral saturate
    float lastOutput = 0;
    for (int i = 1; i <= 20; i++) {
        lastOutput = pid->compute(50.0, 0.0, i * 1000);
    }

    // Output should be capped at PWM_MAX_PID_OUTPUT
    TEST_ASSERT_TRUE(lastOutput <= PWM_MAX_PID_OUTPUT);
    TEST_ASSERT_TRUE(lastOutput >= PWM_MAX_PID_OUTPUT - 1); // Should be at or near max
}

void test_pid_setlimits_caps_to_pwm_max_pid_output() {
    pid->begin();

    // Try to set various limits
    pid->setLimits(0, 100);
    TEST_ASSERT_EQUAL_FLOAT(PWM_MAX_PID_OUTPUT, pid->getOutputMax());

    pid->setLimits(0, PWM_MAX_PID_OUTPUT);
    TEST_ASSERT_EQUAL_FLOAT(PWM_MAX_PID_OUTPUT, pid->getOutputMax());

    pid->setLimits(0, 20);
    TEST_ASSERT_EQUAL_FLOAT(20, pid->getOutputMax()); // Should allow lower limits
}

// ==================== Anti-Windup Tests ====================

void test_pid_limits_output_to_range() {
    pid->begin();
    pid->setProfile(PIDProfile::STRONG);
    pid->setLimits(0, 100); // Will be capped to PWM_MAX_PID_OUTPUT (30)

    pid->compute(50.0, 0.0, 0);

    float output = pid->compute(50.0, 0.0, 1000); // Huge error

    TEST_ASSERT_TRUE(output <= PWM_MAX_PID_OUTPUT);
    TEST_ASSERT_TRUE(output >= 0);
}

void test_pid_anti_windup_prevents_excessive_integral() {
    pid->begin();
    pid->setProfile(PIDProfile::NORMAL);
    pid->setLimits(0, 100); // Will be capped to PWM_MAX_PID_OUTPUT (30)

    pid->compute(50.0, 0.0, 0);

    // Let integral saturate
    for (int i = 1; i <= 10; i++) {
        pid->compute(50.0, 0.0, i * 1000);
    }

    // Now bring temp close to setpoint quickly
    pid->compute(50.0, 49.0, 11000);

    // Output should drop quickly without excessive overshoot from integral
    float output = pid->compute(50.0, 50.0, 12000);

    // At setpoint, output should be reasonable (not maxed from windup)
    TEST_ASSERT_TRUE(output < PWM_MAX_PID_OUTPUT);
}

// ==================== Temperature-Aware Slowdown Tests ====================

void test_pid_slows_near_max_temp() {
    pid->begin();
    pid->setProfile(PIDProfile::STRONG);
    pid->setLimits(0, 255); // Will be capped to PWM_MAX_PID_OUTPUT
    pid->setMaxAllowedTemp(90.0);

    // At safe temperature (50°C from max)
    pid->compute(90.0, 40.0, 0);
    float outputSafe = pid->compute(90.0, 40.0, 1000);

    // At 87°C (3°C from max, within slowdown margin)
    pid->reset();
    pid->compute(90.0, 87.0, 0);
    float outputNearMax = pid->compute(90.0, 87.0, 1000);

    // Output near max should be significantly reduced
    TEST_ASSERT_TRUE(outputNearMax < outputSafe * 0.7);
}

void test_pid_stops_at_max_temp() {
    pid->begin();
    pid->setProfile(PIDProfile::STRONG);
    pid->setLimits(0, 255);
    pid->setMaxAllowedTemp(90.0);

    pid->compute(90.0, 85.0, 0);

    // At max temp
    float output = pid->compute(90.0, 90.0, 1000);

    TEST_ASSERT_EQUAL_FLOAT(0.0, output);
}

void test_pid_stops_above_max_temp() {
    pid->begin();
    pid->setProfile(PIDProfile::STRONG);
    pid->setLimits(0, 255);
    pid->setMaxAllowedTemp(90.0);

    pid->compute(90.0, 85.0, 0);

    // Above max temp
    float output = pid->compute(90.0, 92.0, 1000);

    TEST_ASSERT_EQUAL_FLOAT(0.0, output);
}

void test_pid_scales_linearly_in_slowdown_margin() {
    pid->begin();
    pid->setProfile(PIDProfile::NORMAL);
    pid->setLimits(0, 255);
    pid->setMaxAllowedTemp(90.0);

    // At 88°C (2°C from max)
    pid->compute(90.0, 88.0, 0);
    float output88 = pid->compute(90.0, 88.0, 1000);

    // At 86°C (4°C from max)
    pid->reset();
    pid->compute(90.0, 86.0, 0);
    float output86 = pid->compute(90.0, 86.0, 1000);

    // Output at 86°C should be about 2x output at 88°C
    // (4°C margin vs 2°C margin in 5°C slowdown zone)
    float ratio = output86 / output88;
//    Serial.print("Output at 86°C: ");
//    Serial.println(output86);
//    Serial.print("Output at 88°C: ");
//    Serial.println(output88);
//    Serial.print("Ratio (86°C / 88°C): ");
//    Serial.println(ratio);

    TEST_ASSERT_TRUE(ratio > 2 && ratio < 4.5);
}

// ==================== Reset Tests ====================

void test_pid_reset_clears_integral() {
    pid->begin();
    pid->setProfile(PIDProfile::NORMAL);
    pid->setLimits(0, 30);

    pid->compute(50.0, 45.0, 0);

    // Accumulate integral
    for (int i = 1; i <= 5; i++) {
        pid->compute(50.0, 45.0, i * 1000);
    }

    float outputBeforeReset = pid->compute(50.0, 45.0, 6000);

    // Reset
    pid->reset();

    // First compute after reset (initialization)
    pid->compute(50.0, 45.0, 7000);

    // Second compute should have much less output (no accumulated integral)
    float outputAfterReset = pid->compute(50.0, 45.0, 8000);
//    Serial.print("Output before reset: "); Serial.println(outputBeforeReset);
//    Serial.print("Output after reset: "); Serial.println(outputAfterReset);

    TEST_ASSERT_TRUE(outputAfterReset <= outputBeforeReset);
}

void test_pid_reset_clears_derivative_filter() {
    pid->begin();
    pid->setProfile(PIDProfile::STRONG);
    pid->setLimits(0, 255);

    pid->compute(50.0, 30.0, 0);
    pid->compute(50.0, 35.0, 1000);
    pid->compute(50.0, 40.0, 2000);

    pid->reset();

    // After reset, first compute should initialize
    float output = pid->compute(50.0, 45.0, 3000);
    TEST_ASSERT_EQUAL_FLOAT(0.0, output);
}

// ==================== Profile Comparison Tests ====================

void test_pid_soft_profile_gentler_than_normal() {
    // Test with SOFT profile
    PIDController pidSoft;
    pidSoft.begin();
    pidSoft.setProfile(PIDProfile::SOFT);
    pidSoft.setLimits(0, 255);

    pidSoft.compute(50.0, 40.0, 0);
    float outputSoft = pidSoft.compute(50.0, 40.0, 1000);

    // Test with NORMAL profile
    PIDController pidNormal;
    pidNormal.begin();
    pidNormal.setProfile(PIDProfile::NORMAL);
    pidNormal.setLimits(0, 255);

    pidNormal.compute(50.0, 40.0, 0);
    float outputNormal = pidNormal.compute(50.0, 40.0, 1000);

    // SOFT should produce less output than NORMAL
    TEST_ASSERT_TRUE(outputSoft < outputNormal);
}

void test_pid_strong_profile_more_aggressive_than_normal() {
    // Test with NORMAL profile
    PIDController pidNormal;
    pidNormal.begin();
    pidNormal.setProfile(PIDProfile::NORMAL);
    pidNormal.setLimits(0, 255);

    pidNormal.compute(50.0, 40.0, 0);
    float outputNormal = pidNormal.compute(50.0, 40.0, 1000);

    // Test with STRONG profile
    PIDController pidStrong;
    pidStrong.begin();
    pidStrong.setProfile(PIDProfile::STRONG);
    pidStrong.setLimits(0, 255);

    pidStrong.compute(50.0, 40.0, 0);
    float outputStrong = pidStrong.compute(50.0, 40.0, 1000);

    // STRONG should produce more output than NORMAL
    TEST_ASSERT_TRUE(outputStrong > outputNormal);
}

// ==================== Edge Cases ====================

void test_pid_handles_zero_time_delta() {
    pid->begin();
    pid->setProfile(PIDProfile::NORMAL);
    pid->setLimits(0, 255);

    pid->compute(50.0, 40.0, 0);
    float output1 = pid->compute(50.0, 40.0, 1000);

    // Same timestamp - should return current integral without change
    float output2 = pid->compute(50.0, 45.0, 1000);

    // Should not crash and should return reasonable value
    TEST_ASSERT_TRUE(output2 >= 0 && output2 <= PWM_MAX_PID_OUTPUT);
}

void test_pid_handles_negative_error() {
    pid->begin();
    pid->setProfile(PIDProfile::NORMAL);
    pid->setLimits(0, 255);

    // Temperature above setpoint
    pid->compute(50.0, 60.0, 0);
    float output = pid->compute(50.0, 60.0, 1000);

    // Should produce low/zero output
    TEST_ASSERT_TRUE(output < PWM_MAX_PID_OUTPUT / 2);
}

void test_pid_setpoint_change_no_derivative_kick() {
    pid->begin();
    pid->setProfile(PIDProfile::STRONG); // High Kd
    pid->setLimits(0, 255);

    // Stable at setpoint
    pid->compute(50.0, 50.0, 0);
    pid->compute(50.0, 50.0, 1000);
    float outputStable = pid->compute(50.0, 50.0, 2000);

    // Sudden setpoint change (but temp unchanged)
    float outputAfterSetpointChange = pid->compute(60.0, 50.0, 3000);

    // Derivative is on measurement, not error, so no kick from setpoint change
    // Output should increase due to proportional term only
    TEST_ASSERT_TRUE(outputAfterSetpointChange > outputStable);
}

// ==================== Integration Tests with PWM_MAX_PID_OUTPUT ====================

void test_pid_typical_heating_with_limited_output() {
    pid->begin();
    pid->setProfile(PIDProfile::NORMAL);
    pid->setLimits(0, 100); // Will be capped to PWM_MAX_PID_OUTPUT

    // Starting cold, large error
    pid->compute(50.0, 20.0, 0);

    // Should quickly reach max output but not exceed PWM_MAX_PID_OUTPUT
    float output1 = pid->compute(50.0, 20.0, 1000);
    float output2 = pid->compute(50.0, 25.0, 2000);
    float output3 = pid->compute(50.0, 30.0, 3000);

    TEST_ASSERT_TRUE(output1 <= PWM_MAX_PID_OUTPUT);
    TEST_ASSERT_TRUE(output2 <= PWM_MAX_PID_OUTPUT);
    TEST_ASSERT_TRUE(output3 <= PWM_MAX_PID_OUTPUT);

    // As temp approaches setpoint, output should decrease
    float output4 = pid->compute(50.0, 45.0, 4000);
    float output5 = pid->compute(50.0, 49.0, 5000);

    TEST_ASSERT_TRUE(output4 < output3);
    TEST_ASSERT_TRUE(output5 < output4);
}

// ==================== Main Test Runner ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_pid_initializes);
    RUN_TEST(test_pid_starts_with_zero_output);

    // Profile tests
    RUN_TEST(test_pid_accepts_profile_changes);

    // Basic PID functionality
    RUN_TEST(test_pid_produces_output_after_first_compute);
    RUN_TEST(test_pid_proportional_term_responds_to_error);
    RUN_TEST(test_pid_accumulates_integral);
    RUN_TEST(test_pid_derivative_opposes_change);

    // PWM_MAX_PID_OUTPUT limit tests
    RUN_TEST(test_pid_respects_pwm_max_pid_output_limit);
    RUN_TEST(test_pid_output_never_exceeds_pwm_max_pid_output);
    RUN_TEST(test_pid_integral_windup_limited_to_pwm_max_pid_output);
    RUN_TEST(test_pid_setlimits_caps_to_pwm_max_pid_output);

    // Anti-windup
    RUN_TEST(test_pid_limits_output_to_range);
    RUN_TEST(test_pid_anti_windup_prevents_excessive_integral);

    // Temperature-aware slowdown
    RUN_TEST(test_pid_slows_near_max_temp);
    RUN_TEST(test_pid_stops_at_max_temp);
    RUN_TEST(test_pid_stops_above_max_temp);
    RUN_TEST(test_pid_scales_linearly_in_slowdown_margin);

    // Reset
    RUN_TEST(test_pid_reset_clears_integral);
    RUN_TEST(test_pid_reset_clears_derivative_filter);

    // Profile comparison
    RUN_TEST(test_pid_soft_profile_gentler_than_normal);
    RUN_TEST(test_pid_strong_profile_more_aggressive_than_normal);

    // Edge cases
    RUN_TEST(test_pid_handles_zero_time_delta);
    RUN_TEST(test_pid_handles_negative_error);
    RUN_TEST(test_pid_setpoint_change_no_derivative_kick);

    return UNITY_END();
}