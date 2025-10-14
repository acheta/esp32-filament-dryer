#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

/**
 * TestConfig.h - Test-Specific Configuration
 *
 * This file contains fixed configuration values for unit tests.
 * These values are decoupled from production Config.h to ensure
 * tests remain stable when production configurations change.
 *
 * IMPORTANT: These values should match the test expectations,
 * NOT necessarily the production values in src/Config.h
 */

// ==================== Preset Configurations ====================

// PLA Preset
constexpr float TEST_PRESET_PLA_TEMP = 50.0;
constexpr uint32_t TEST_PRESET_PLA_TIME = 18000;  // 5 hours in seconds
constexpr float TEST_PRESET_PLA_OVERSHOOT = 5.0;

// PETG Preset
constexpr float TEST_PRESET_PETG_TEMP = 65.0;
constexpr uint32_t TEST_PRESET_PETG_TIME = 18000;  // 5 hours in seconds
constexpr float TEST_PRESET_PETG_OVERSHOOT = 5.0;

// Custom Preset Defaults
constexpr float TEST_PRESET_CUSTOM_TEMP = 55.0;
constexpr uint32_t TEST_PRESET_CUSTOM_TIME = 14400;  // 4 hours in seconds
constexpr float TEST_PRESET_CUSTOM_OVERSHOOT = 5.0;

// ==================== Constraint Values ====================

constexpr float TEST_MIN_TEMP = 30.0;
constexpr float TEST_MAX_TEMP = 80.0;
constexpr uint32_t TEST_MIN_TIME_SECONDS = 600;     // 10 minutes
constexpr uint32_t TEST_MAX_TIME_SECONDS = 36000;   // 10 hours
constexpr float TEST_MAX_OVERSHOOT = 10.0;

// ==================== PWM Configuration ====================

constexpr uint8_t TEST_PWM_MAX = 255;
constexpr uint8_t TEST_PWM_MAX_PID_OUTPUT = 30;  // Safety limit for PID output

// ==================== State Persistence ====================

constexpr uint32_t TEST_STATE_SAVE_INTERVAL = 60000;  // Save every 60 seconds

// ==================== PID Profiles ====================

// SOFT profile
constexpr float TEST_PID_SOFT_KP = 2.0;
constexpr float TEST_PID_SOFT_KI = 0.05;
constexpr float TEST_PID_SOFT_KD = 1.0;

// NORMAL profile
constexpr float TEST_PID_NORMAL_KP = 4.0;
constexpr float TEST_PID_NORMAL_KI = 0.1;
constexpr float TEST_PID_NORMAL_KD = 2.0;

// STRONG profile
constexpr float TEST_PID_STRONG_KP = 6.0;
constexpr float TEST_PID_STRONG_KI = 0.15;
constexpr float TEST_PID_STRONG_KD = 3.0;

#endif
