#ifndef CONFIG_H
#define CONFIG_H

#ifndef UNIT_TEST
    #include <Arduino.h>
#else
    // Mock Arduino functions for native testing
    #include "../test/mocks/arduino_mock.h"
#endif

// ============================================================================
// BOARD SELECTION - Uncomment ONE of these
// ============================================================================
#define ESP32_C3_BOARD    // ESP32-C3 Super Mini
// #define ESP32_S3_BOARD    // ESP32-S3 DevKit


#ifdef ESP32_C3_BOARD
// ============================================================================
// ESP32-C3 SUPER MINI PINOUT (Based on actual board layout)
// ============================================================================
// Available GPIOs: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21
// Hardware I2C: SDA=GPIO8 (conflicts with LED!), SCL=GPIO9
// Hardware SPI: MISO=GPIO5, MOSI=GPIO6, SS=GPIO7
// UART: TX=GPIO20, RX=GPIO21
// ADC: GPIO0-4 (A0-A4)
// Built-in LED: GPIO8 (active LOW, conflicts with I2C SDA!)
// Boot button: GPIO9 (conflicts with I2C SCL!)
// ============================================================================

// Status LED - We'll use this sparingly or not at all due to I2C conflict
constexpr uint8_t STATUS_LED_PIN = 8;      // Shared with I2C SDA!

// Heater Control - Use a free GPIO with PWM capability
constexpr uint8_t HEATER_PWM_PIN = 10;     // Free GPIO, PWM capable

// Temperature Sensors
constexpr uint8_t HEATER_TEMP_PIN = 4;     // DS18B20 OneWire (A4)

// I2C Bus (for AM2320 + OLED)
// OPTION A: Use hardware I2C but sacrifice LED
constexpr uint8_t I2C_SDA_PIN = 8;         // Hardware I2C (shares with LED!)
constexpr uint8_t I2C_SCL_PIN = 9;         // Hardware I2C (shares with BOOT!)

// OPTION B: Use software I2C on different pins (recommended)
// Uncomment these and comment out the lines above if you want the LED working
// constexpr uint8_t I2C_SDA_PIN = 6;      // Software I2C (was MOSI)
// constexpr uint8_t I2C_SCL_PIN = 7;      // Software I2C (was SS)

// Buttons - Use analog-capable pins with internal pull-ups
constexpr uint8_t BUTTON_SET_PIN = 5;      // GPIO5
constexpr uint8_t BUTTON_UP_PIN = 6;       // GPIO6
constexpr uint8_t BUTTON_DOWN_PIN = 7;     // GPIO7

// Buzzer
constexpr uint8_t BUZZER_PIN = 3;          // A3

// Boot button (built-in) - GPIO9 also used for SCL!
constexpr uint8_t BOOT_BUTTON_PIN = 9;

// Fan pin
constexpr uint8_t FAN_PIN = 2;

#endif // ESP32_C3_BOARD

// ============================================================================
// ESP32-S3 DEVKIT PINOUT
// ============================================================================
#ifdef ESP32_S3_BOARD

// Available GPIOs: 0-21, 26-48 (avoid 26-32 for Flash/PSRAM)
// Strapping pins: 0, 3, 45, 46 (use with caution)
// USB: GPIO 19 (D-), GPIO 20 (D+) - avoid to keep USB working

// Status LED (RGB LED on many S3 DevKits)
constexpr uint8_t STATUS_LED_PIN = 48;     // Or use GPIO2 for simple LED

// Heater Control
constexpr uint8_t HEATER_PWM_PIN = 16;      // Safe GPIO, PWM capable

// Temperature Sensors
constexpr uint8_t HEATER_TEMP_PIN = 4;     // DS18B20 OneWire

// I2C Bus (for AM2320 + OLED) - Default hardware I2C
constexpr uint8_t I2C_SDA_PIN = 8;         // Default hardware I2C SDA
constexpr uint8_t I2C_SCL_PIN = 9;         // Default hardware I2C SCL

// Buttons (with internal pull-ups)
constexpr uint8_t BUTTON_SET_PIN = 5;      // GPIO5
constexpr uint8_t BUTTON_UP_PIN = 6;       // GPIO6
constexpr uint8_t BUTTON_DOWN_PIN = 7;     // GPIO7

// Buzzer
constexpr uint8_t BUZZER_PIN = 26;         // not implemented/tested

// Boot button (built-in)
constexpr uint8_t BOOT_BUTTON_PIN = 0;     // Strapping pin

// Fan pin
constexpr uint8_t FAN_PIN = 2;

#endif // ESP32_S3_BOARD

// ============================================================================
// COMMON CONFIGURATION (Both boards)
// ============================================================================


// ==================== Timing Constants ====================

// Update intervals (milliseconds)
constexpr uint32_t HEATER_TEMP_INTERVAL = 1000;
constexpr uint32_t BOX_DATA_INTERVAL = 2000;
constexpr uint32_t PID_UPDATE_INTERVAL = 500;
constexpr uint32_t STATE_SAVE_INTERVAL = 60000; // Save every 60 seconds
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

constexpr uint32_t HEATER_PWM_PERIOD_MS = 2000;   // 2 second period
constexpr float HEATER_PWM_FREQ = 1000.0 / HEATER_PWM_PERIOD_MS;  // 0.2 Hz (for reference only)

constexpr uint8_t PWM_MIN = 0;
constexpr uint8_t PWM_MAX = 100;  // Scale to 0-100 for simplicity, while using software PWM
constexpr uint8_t PWM_MAX_PID_OUTPUT = 50;  // The heater is too powerful with thermal momentum - limit max PID output to 30%



// ==================== PID Configuration ====================

// PID Profiles
struct PIDTuning {
    float kp;
    float ki;
    float kd;
};

constexpr PIDTuning PID_SOFT = {1.0, 0.2, 2.0};    // Very gentle
constexpr PIDTuning PID_NORMAL = {2.0, 0.3, 3.0};  // Moderate
constexpr PIDTuning PID_STRONG = {4.5, 0.6, 4.0};  // Still careful

// PID control parameters
constexpr float PID_DERIVATIVE_FILTER_ALPHA = 0.9;  // Low-pass filter coefficient
constexpr float PID_TEMP_SLOWDOWN_MARGIN = 5.0;    // Start scaling within margin of max

// Box temperature control parameters
constexpr float BOX_TEMP_APPROACH_MARGIN = 3.0;    // °C - Start conservative heating when box is this close to target
constexpr float MAX_BOX_TEMP_OVERSHOOT = 2.0;      // °C - Maximum allowed box temperature overshoot above target
constexpr float MIN_HEATER_TEMP_MARGIN = 0.5;      // °C - Deadband for minimum heater temperature control at steady state

// Steady-state control parameters
constexpr float STEADY_STATE_TOLERANCE = 0.5;      // °C - Box error tolerance for steady-state detection
constexpr uint32_t STEADY_STATE_TIME_MS = 10000;   // Time at target before considering steady-state
constexpr float STEADY_STATE_OUTPUT_FILTER = 0.8; // Exponential filter for steady-state output tracking
constexpr float STEADY_STATE_OUTPUT_BAND = 3.0;    // ±% around steady-state output to maintain
constexpr float STEADY_STATE_MIN_OUTPUT = 20.0;    // % - Minimum output to maintain when near target (baseline)

// Heater-Box correlation parameters
constexpr float HEATER_BOX_LEAD_TIME_SEC = 20.0;   // Heater leads box by ~20 seconds
constexpr float HEATER_BOX_CORRELATION_FILTER = 0.9; // Filter for correlation tracking

// Heater momentum compensation (heater has significant thermal mass + sensor lag)
constexpr float HEATER_MOMENTUM_THRESHOLD = -0.1;  // °C/s - When heater cools this fast, apply compensation
constexpr float HEATER_MOMENTUM_GAIN = 10.0;       // Boost output by this much per °C/s of heater cooling
constexpr float MIN_OUTPUT_NEAR_TARGET = 19.0;     // % - Never drop below this when within 2°C of target

// Baseline insufficiency compensation (when baseline output is insufficient to maintain target)
constexpr uint32_t BASELINE_ENFORCEMENT_THRESHOLD_MS = 5000;  // Wait 5 seconds before detecting insufficiency
constexpr float BASELINE_BOOST_GAIN = 15.0;        // Boost output by this much per °C/s of box cooling
constexpr float MAX_BASELINE_BOOST = 10.0;         // % - Maximum additional boost above baseline

// ==================== Preset Configurations ====================

#ifdef UNIT_TEST
    // Use test-specific values to decouple tests from production config changes
    #include "../test/TestConfig.h"

    // PLA preset
    constexpr float PRESET_PLA_TEMP = TEST_PRESET_PLA_TEMP;
    constexpr uint32_t PRESET_PLA_TIME = TEST_PRESET_PLA_TIME;
    constexpr float PRESET_PLA_OVERSHOOT = TEST_PRESET_PLA_OVERSHOOT;

    // PETG preset
    constexpr float PRESET_PETG_TEMP = TEST_PRESET_PETG_TEMP;
    constexpr uint32_t PRESET_PETG_TIME = TEST_PRESET_PETG_TIME;
    constexpr float PRESET_PETG_OVERSHOOT = TEST_PRESET_PETG_OVERSHOOT;

    // Default custom preset
    constexpr float PRESET_CUSTOM_TEMP = TEST_PRESET_CUSTOM_TEMP;
    constexpr uint32_t PRESET_CUSTOM_TIME = TEST_PRESET_CUSTOM_TIME;
    constexpr float PRESET_CUSTOM_OVERSHOOT = TEST_PRESET_CUSTOM_OVERSHOOT;

    // ABS preset (not used in tests, but defined for completeness)
    constexpr float PRESET_ABS_TEMP = 75.0;
    constexpr uint32_t PRESET_ABS_TIME = 18000;
    constexpr float PRESET_ABS_OVERSHOOT = 10.0;
#else
    // Production values

    // PLA preset
    constexpr float PRESET_PLA_TEMP = 51.0;
    constexpr uint32_t PRESET_PLA_TIME = 5*60*60; // 5 hours
    constexpr float PRESET_PLA_OVERSHOOT = 10.0;

    // PETG preset
    constexpr float PRESET_PETG_TEMP = 65.0;
    constexpr uint32_t PRESET_PETG_TIME = 5*60*60; // 5 hours
    constexpr float PRESET_PETG_OVERSHOOT = 10.0;

    // ABS preset
    constexpr float PRESET_ABS_TEMP = 75.0;
    constexpr uint32_t PRESET_ABS_TIME = 5*60*60; // 5 hours
    constexpr float PRESET_ABS_OVERSHOOT = 10.0;

    // Default custom preset
    constexpr float PRESET_CUSTOM_TEMP = 50.0;
    constexpr uint32_t PRESET_CUSTOM_TIME = 5*60*60; // 5 hours
    constexpr float PRESET_CUSTOM_OVERSHOOT = 10.0;
#endif

// ==================== Storage Configuration ====================

#define SETTINGS_FILE "/settings.json"
#define RUNTIME_FILE "/runtime.json"

// ==================== Safety Configuration ====================

constexpr uint32_t WATCHDOG_TIMEOUT = 10000;  // 10 seconds

// ==================== Display Configuration ====================

constexpr uint8_t DISPLAY_WIDTH = 128;
constexpr uint8_t DISPLAY_HEIGHT = 32;
constexpr uint8_t DISPLAY_FONT_SIZE = 1;

// Button timing
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;
constexpr uint32_t BUTTON_CLICK_MS = 400;
constexpr uint32_t BUTTON_LONG_PRESS_MS = 1000;

// UI timing
constexpr uint32_t MENU_INACTIVITY_TIMEOUT_MS = 30000;  // 30 seconds

#endif