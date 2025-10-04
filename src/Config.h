#ifndef CONFIG_H
#define CONFIG_H

#ifndef UNIT_TEST
    #include <Arduino.h>
#else
    // Mock Arduino functions for native testing
    #include "../test/mocks/arduino_mock.h"
#endif

// ==================== Hardware Pins ====================

// Heater
#define HEATER_PWM_PIN 25
#define HEATER_PWM_CHANNEL 0
#define HEATER_PWM_FREQ 1000

// Sensors
#define HEATER_TEMP_PIN 4        // DS18B20 OneWire
#define BOX_SENSOR_SDA 21        // AM2320 I2C
#define BOX_SENSOR_SCL 22        // AM2320 I2C

// Buttons
#define BUTTON_SET_PIN 32
#define BUTTON_UP_PIN 33
#define BUTTON_DOWN_PIN 34

// Display (I2C OLED)
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDRESS 0x3C

// Sound
#define BUZZER_PIN 26

// ==================== Timing Constants ====================

// Update intervals (milliseconds)
constexpr uint32_t HEATER_TEMP_INTERVAL = 500;
constexpr uint32_t BOX_DATA_INTERVAL = 2000;
constexpr uint32_t PID_UPDATE_INTERVAL = 500;
constexpr uint32_t STATE_SAVE_INTERVAL = 1000;
constexpr uint32_t DISPLAY_UPDATE_INTERVAL = 200;
constexpr uint32_t SENSOR_TIMEOUT = 5000;

// ==================== Temperature Limits ====================

// Operational limits
constexpr float MIN_TEMP = 30.0;              // Minimum setpoint
constexpr float MAX_BOX_TEMP = 80.0;          // Maximum box temperature
constexpr float MAX_HEATER_TEMP = 90.0;       // Maximum heater temperature (80 + 10 overshoot)
constexpr float DEFAULT_MAX_OVERSHOOT = 10.0; // Default max overshoot above target

// ==================== Time Limits ====================

constexpr uint32_t MAX_TIME_SECONDS = 36000;  // 10 hours
constexpr uint32_t MIN_TIME_SECONDS = 600;    // 10 minutes
constexpr uint32_t POWER_RECOVERY_TIMEOUT = 300000; // 5 minutes

// ==================== PWM Configuration ====================

constexpr uint32_t PWM_PERIOD_MS = 5000;      // 5 second PWM period
constexpr uint8_t PWM_RESOLUTION = 8;         // 8-bit (0-255)
constexpr uint8_t PWM_MIN = 0;
constexpr uint8_t PWM_MAX = 255;

// ==================== PID Configuration ====================

// PID Profiles
struct PIDTuning {
    float kp;
    float ki;
    float kd;
};

constexpr PIDTuning PID_SOFT = {2.0, 0.5, 1.0};
constexpr PIDTuning PID_NORMAL = {4.0, 1.0, 2.0};
constexpr PIDTuning PID_STRONG = {6.0, 1.5, 3.0};

// PID control parameters
constexpr float PID_DERIVATIVE_FILTER_ALPHA = 0.2;  // Low-pass filter coefficient
constexpr float PID_TEMP_SLOWDOWN_MARGIN = 5.0;    // Start scaling within 5°C of max

// ==================== Preset Configurations ====================

// PLA preset
constexpr float PRESET_PLA_TEMP = 50.0;
constexpr uint32_t PRESET_PLA_TIME = 14400;  // 4 hours
constexpr float PRESET_PLA_OVERSHOOT = 10.0;

// PETG preset
constexpr float PRESET_PETG_TEMP = 65.0;
constexpr uint32_t PRESET_PETG_TIME = 18000; // 5 hours
constexpr float PRESET_PETG_OVERSHOOT = 10.0;

// Default custom preset
constexpr float PRESET_CUSTOM_TEMP = 50.0;
constexpr uint32_t PRESET_CUSTOM_TIME = 14400;
constexpr float PRESET_CUSTOM_OVERSHOOT = 10.0;

// ==================== Storage Configuration ====================

#define SETTINGS_FILE "/settings.json"
#define RUNTIME_FILE "/runtime.json"

// ==================== Safety Configuration ====================

constexpr uint32_t WATCHDOG_TIMEOUT = 10000;  // 10 seconds

// ==================== Display Configuration ====================

constexpr uint8_t DISPLAY_WIDTH = 128;
constexpr uint8_t DISPLAY_HEIGHT = 64;
constexpr uint8_t DISPLAY_FONT_SIZE = 1;

#endif