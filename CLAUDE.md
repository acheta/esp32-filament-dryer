# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 Arduino-based filament dryer with temperature/humidity control. This is an embedded C++ project using PlatformIO with a testability-first architecture featuring interface-based design, dependency injection, and comprehensive mocking for unit tests.

## Build & Development Commands

### Building
```bash
# Build for ESP32-S3
pio run -e esp32-s3-n8r2

# Build for ESP32-C3
pio run -e esp32-c3-super-mini

# Upload to device
pio run -e esp32-s3-n8r2 -t upload
```

### Testing
```bash
# Run all unit tests (Windows)
pio test -e native -v -f test_dryer_integration -f test_pid_controller -f test_safety_monitor -f test_sensor_integration -f test_display -f test_heater_control -f test_fan_control -f test_settings_storage -f test_button_manager -f test_menu_controller -f test_ui_controller

# Run all unit tests (Linux/Unix container)
pio test -e native-linux -v -f test_dryer_integration -f test_pid_controller -f test_safety_monitor -f test_sensor_integration -f test_display -f test_heater_control -f test_fan_control -f test_settings_storage -f test_button_manager -f test_menu_controller -f test_ui_controller

# Run specific test (Windows)
pio test -e native -f test_pid_controller

# Run specific test (Linux/Unix container)
pio test -e native-linux -f test_pid_controller

# Note: The following test suites are planned but not yet implemented:
# - test_sound_controller
```

### Monitoring
```bash
# Serial monitor
pio device monitor -b 115200
```

## Architecture Overview

### Design Principles

**Testability First**: All components use interface-based design (interfaces prefixed with `I`) with dependency injection. No component creates its own dependencies. Single-file headers (`.h` only, no `.cpp`) for Unity PIO compatibility.

**Time Injection**: All `update()` methods accept `currentMillis` as a parameter instead of calling `millis()` internally. This enables deterministic testing.

**Communication Pattern**:
- **Callbacks (Push)**: For time-critical/event-driven data using `std::function<>`. Components expose `register*Callback()` methods.
- **Getters (Pull)**: For on-demand cached state access.

**Configuration**: All constants (timing, limits, tuning) centralized in `Config.h`. Never hardcode values.

### Component Hierarchy

```
Dryer (Main Orchestrator)
├── SensorManager
│   ├── HeaterTempSensor (DS18B20)
│   └── BoxTempHumiditySensor (AM2320)
├── PIDController (temperature control with anti-windup)
├── HeaterControl (software PWM controller)
├── SafetyMonitor (passive watchdog)
├── FanControl (optional cooling fan)
├── SettingsStorage (LittleFS + JSON)
└── SoundController (buzzer, optional)

UIController (UI coordinator)
├── MenuController (menu state machine)
├── ButtonManager (OneButton wrapper)
└── OLEDDisplay (SSD1306 driver)
```

### Key Data Flows

**Sensor → Control**: SensorManager reads sensors at intervals → fires callbacks → PIDController computes → Dryer sets HeaterControl PWM

**User Input**: Button press → ButtonManager → UIController → MenuController → Dryer methods

**Safety**: SensorManager/SafetyMonitor detect violations → EmergencyStopCallback → Dryer stops heater/fan → FAILED state

### Critical Implementation Details

**HeaterControl Software PWM**: Must call `update(currentMillis)` frequently (< 100ms) in main loop. Uses software timing with period from `HEATER_PWM_PERIOD_MS` because hardware LEDC can't achieve long periods needed for SSR relay longevity.

**PID Control Strategy**: PID controls **box temperature** (primary control variable), not heater temperature. The heater is the actuator. Critical points:
- `compute(setpoint, boxTemp, heaterTemp, currentMillis)` signature - requires BOTH temperatures
- **Two-phase heater limiting** prevents box temperature overshoot:
  - **Aggressive phase**: Box >5°C from target → heater allowed up to `maxAllowedTemp` (e.g., 60°C)
  - **Conservative phase**: Box ≤5°C from target → heater limit reduces proportionally
  - **At target**: Heater limited to `setpoint + MAX_BOX_TEMP_OVERSHOOT` (e.g., 52°C for 50°C target)
- **Minimum heater temperature control** ensures steady-state stability:
  - When box at target, heater maintained at or above `setpoint - MIN_HEATER_TEMP_MARGIN`
  - Prevents heater from cooling below box target temperature
  - Eliminates temperature oscillations at steady state
- Config constants: `BOX_TEMP_APPROACH_MARGIN` (5°C), `MAX_BOX_TEMP_OVERSHOOT` (2°C), `MIN_HEATER_TEMP_MARGIN` (0.5°C)
- PID error calculated from box temperature: `error = setpoint - boxTemp`
- Additional temperature-aware slowdown when heater approaches its dynamic limit

**PID Output Limiting**: Output capped to `PWM_MAX_PID_OUTPUT` (not `PWM_MAX`) to prevent overshoot. Temperature-aware slowdown scales output when approaching max temp. See specification.MD lines 54-84 for anti-windup, predictive cooling, and two-phase heater limiting details.

**State Persistence**: Dryer saves runtime state every `STATE_SAVE_INTERVAL` to enable power loss recovery (POWER_RECOVERED state).

**Fan Behavior**: Starts with heater in RUNNING, stays on during PAUSED (for cooling), stops in READY/FINISHED/FAILED.

## File Structure

- `src/main.cpp` - Setup, loop, serial command handler, watchdog init
- `src/Config.h` - All pin definitions, constants, timing values, presets
- `src/Types.h` - Shared enums/structs (DryerState, PresetType, PIDProfile, etc.)
- `src/interfaces/` - All interface definitions (prefix `I`)
- `src/control/` - HeaterControl, PIDController, SafetyMonitor, FanControl
- `src/sensors/` - SensorManager, HeaterTempSensor, BoxTempHumiditySensor
- `src/userInterface/` - UIController, MenuController, ButtonManager, OLEDDisplay
- `src/storage/` - SettingsStorage
- `test/` - Unit tests and mocks

## Adding New Components

1. Create interface in `interfaces/I<ComponentName>.h` with pure virtual methods
2. Implement in appropriate folder (control/, sensors/, userInterface/, storage/)
3. Constructor takes dependencies as pointers (never create internally)
4. Add `update(uint32_t currentMillis)` if time-dependent
5. Use `std::function<>` callbacks and provide `register*Callback()` methods
6. Store callbacks in `std::vector<>` and check `if (callback)` before invoking
7. Create mock in `test/mocks/Mock<ComponentName>.h`
8. Write unit tests with time injection
9. Reference Config.h constants by name, never hardcode

## Testing Guidelines

- All tests use mocks for dependencies (no hardware access)
- Time is injected via parameters, never use `millis()` or `delay()`
- Test structure: Arrange-Act-Assert
- Mocks track calls and allow setting return values
- Integration tests verify component interactions

## Serial Commands

Available in runtime via serial monitor:
- `start`, `pause`, `resume`, `stop`, `reset` - State control
- `preset pla|petg|custom` - Select preset
- `pid soft|normal|strong` - Set PID profile
- `sound on|off` - Toggle sound
- `status` - Print current state
- `help` - Show all commands

## Important Notes

- **Board selection**: Edit `Config.h` to uncomment either `ESP32_C3_BOARD` or `ESP32_S3_BOARD`
- **Watchdog**: Hardware watchdog (10s timeout) auto-resets if loop() hangs
- **Detailed architecture**: See `specification.MD` for complete design documentation including state machines, callback flows, menu tree, PID algorithms, and safety architecture
- **TODO items**: See `TODO.md` for list of missing implementations and planned features, particularly SoundController implementation and missing test suites
