# ESP32 Arduino Dryer Project - Architecture Guidelines

## Core Design Principles

### 1. Testability First
- **Single-file headers**: All classes in single `.h` files (no separate `.cpp`) for Unity PIO compatibility
- **Interface-based design**: Use abstract interfaces (prefix with `I`) for all major components
- **Dependency injection**: Pass dependencies through constructors, never create them internally
- **Factory pattern**: Per-component factories to create either production or mock objects
- **Time injection**: Pass `currentMillis` as parameter to all `update()` methods for deterministic testing

### 2. Communication Pattern
- **Primary: Callbacks** for inter-component communication to enable loose coupling
- **Callback types**: Use `std::function<>` for flexibility
- **Registration pattern**: Components expose `register*Callback()` methods
- **Hybrid approach**:
  - **Push (callbacks)**: For time-critical or event-driven data (sensor updates, state changes, emergencies)
  - **Pull (getters)**: For on-demand access to cached/current state (display updates, stats queries)

### 3. Component Responsibilities

#### **Dryer** (Main Orchestrator)
- Owns all major components via interfaces
- State machine implementation (READY, RUNNING, PAUSED, FINISHED, FAILED, POWER_RECOVERED)
- State transition logic and validation
- **Sets constraints** to respective components (does NOT enforce them):
  - Passes max heater temp to PIDController
  - Passes max box temp to SafetyMonitor
  - Passes min/max temp to MenuController for value clamping
  - Passes max time to MenuController
- Registers callbacks with all components
- Called from `loop()` via `update(currentMillis)`
- Persists runtime state periodically (interval defined in Config.h as `STATE_SAVE_INTERVAL`)
- **Allows preset changes during RUNNING/PAUSED states** - resets timer to 0 when preset changed
- **Timer adjustment**: `adjustRemainingTime(deltaSeconds)` allows adding/subtracting time, clamped to MIN/MAX_TIME_SECONDS
- **Does NOT**: Read sensors directly, control hardware directly, handle UI logic, validate constraints, cap PWM output (PIDController does this)

#### **SensorManager**
- Owns all physical sensors (DS18B20, AM2320)
- Multi-rate reading strategy:
  - Heater temp (DS18B20): Interval defined by `HEATER_TEMP_INTERVAL` in Config.h
  - Box temp/humidity (AM2320): Interval defined by `BOX_DATA_INTERVAL` in Config.h
- Maintains cached readings with timestamps and validity flags
- **Async reading pattern for DS18B20**: Uses `requestConversion()` → wait → `isConversionReady()` → `read()` to avoid blocking
- **Push interface**: Callbacks on new readings
  - `registerHeaterTempCallback(callback)` - fires at heater temp interval
  - `registerBoxDataCallback(callback)` - fires at box data interval
  - `registerSensorErrorCallback(callback)` - fires on failures
- **Pull interface**: Returns cached values on demand
  - `getReadings()`, `getHeaterTemp()`, `getBoxTemp()`, `getBoxHumidity()`
- Detects and reports sensor failures
- **Does NOT**: Process or interpret readings, enforce limits

#### **PIDController**
- **Stateful** - maintains integral accumulation and last box temperature for derivative
- **Controls BOX temperature, not heater temperature** - this is the primary control variable
- Computes PWM output: `compute(setpoint, boxTemp, heaterTemp, currentMillis)`
  - `setpoint`: Target box temperature (e.g., 50°C)
  - `boxTemp`: Current box temperature (primary control variable)
  - `heaterTemp`: Current heater temperature (used for dynamic limiting)
- **Three profiles**: `setProfile(PIDProfile::SOFT | NORMAL | STRONG)` - tuning constants defined in Config.h
- **Output limits**: Capped to `PWM_MAX_PID_OUTPUT` from Config.h (primary power safety limit)
- **Anti-windup protection**:
  - Clamp integral term to output limits before adding to total output
  - When output saturates (hits min/max), stop accumulating integral
  - Reset integral when error changes sign or system restarts
- **Derivative smoothing** (low-pass filter):
  - Use exponential moving average on derivative term to reduce noise sensitivity
  - Filter coefficient defined as `PID_DERIVATIVE_FILTER_ALPHA` in Config.h
  - Derivative calculated on box temperature measurement (not error) to avoid setpoint kick
- **Two-phase heater limiting** (PRIMARY overshoot prevention):
  - **Aggressive phase**: Box far from target (>BOX_TEMP_APPROACH_MARGIN) → heater allowed up to `maxAllowedTemp`
  - **Conservative phase**: Box near target (≤BOX_TEMP_APPROACH_MARGIN) → heater limit reduces proportionally
  - At target: heater limit = `setpoint + MAX_BOX_TEMP_OVERSHOOT` (e.g., 52°C for 50°C target)
  - Within slowdown margin of heater limit: scale output proportionally
  - Emergency stop if heater at or above dynamic limit
- **Minimum heater temperature control** (steady-state stability):
  - When box at or very close to target (≤0.5°C), ensures heater doesn't drop below `setpoint - MIN_HEATER_TEMP_MARGIN`
  - Provides minimum output boost if heater drops too low
  - Prevents oscillations and maintains stable steady-state temperature
  - Only active when box is at target to avoid interfering with heating phase
- **Predictive cooling compensation**:
  - Tracks cooling rate (°C/s) with exponential moving average
  - When cooling faster than `MIN_COOLING_RATE`, predicts future temperature
  - Enhances error term to prevent undershoot during cooldown
  - Uses `PREDICTIVE_HORIZON_SEC` and `PREDICTIVE_GAIN` constants
- `reset()` method to clear state when starting/stopping
- Debug methods: `getCoolingRate()`, `getOutputMax()`
- **Does NOT**: Read sensors, control heater directly

#### **HeaterControl**
- Controls heater via **software PWM** (not hardware LEDC)
- **Must be called frequently**: `update(currentMillis)` required in main loop to maintain PWM timing
- **Software PWM timing**:
  - Period defined by `HEATER_PWM_PERIOD_MS` in Config.h
  - Duty cycle: 0-100 (PWM_MIN to PWM_MAX from Config.h)
  - Tracks cycle start time and pin state internally
- **Simple relay**: Directly applies PWM value from PID without modification
- Methods: `begin()`, `start()`, `stop()`, `emergencyStop()`, `setPWM()`, `isRunning()`, `update()`
- Debug method: `getPinState()` - returns current GPIO state
- **Does NOT**: Scale or modify PWM values, make safety decisions, read sensors
- Trusts PID for proper control (including PWM_MAX_PID_OUTPUT limit) and SafetyMonitor for emergencies

#### **SafetyMonitor**
- **Passive guardian** - monitors but doesn't control
- Subscribes to all sensor updates via callbacks
- Checks hard limits (defined in Config.h):
  - Max heater temp: `MAX_HEATER_TEMP`
  - Max box temp: `MAX_BOX_TEMP`
  - Sensor timeout: `SENSOR_TIMEOUT`
- Triggers `EmergencyStopCallback` on violations
- Dryer's emergency callback stops heater, stops fan, and transitions to FAILED state
- **Hardware backup**: Temperature-rated bimetal switch on main power (physical hardware outside software control)
- **Does NOT**: Control heater or fan directly, make operational decisions

#### **FanControl** (Optional)
- Controls cooling fan relay (ON/OFF)
- **Fan operation rules**:
  - Starts when heater starts (RUNNING state)
  - Keeps running when paused (for cooling)
  - Stops when: finished, failed, reset, or stopped
- Methods: `start()`, `stop()`, `isRunning()`
- **Does NOT**: Make decisions about when to run (Dryer does this)

#### **UIController**
- Owns display, menu controller, button manager
- **Registers button callbacks** with ButtonManager
- Translates button presses to MenuController actions
- Bridges menu selections to Dryer via callbacks
- Subscribes to Dryer stats updates for display
- Triggers sound feedback via SoundController
- Handles display refresh (pull stats on demand)
- **Optimization**: Dirty flag pattern - only updates display when values change
- **Timing consistency**: Stores `currentTime` from `update()` for use in callbacks
- **Two display modes**:
  - **HOME mode**: Shows stats screens (cycle with UP/DOWN)
    - BOX_TEMP (default): Large box temp with "B:" prefix
    - REMAINING: Large remaining time in h:mm:ss
    - HEATER_TEMP: Large heater temp with "H:" prefix
    - STATUS_OVERVIEW: State, Elapsed, Fan, Sound (all size 1)
    - PRESET_CONFIG: Preset, PID, Temp/Overshoot, Target time (all size 1)
    - SENSOR_READINGS: Box, Heater, PID/PWM_MAX, Humidity (all size 1)
  - **MENU mode**: Shows menu navigation
- **Display layout**:
  - Line 1.1 (Y=0): State char (R|>|||F|!|P) + Preset abbrev (PLA|PETG|CUST) in top-right
  - Lines 1.2+2 (Y=8-16): Main value (temperature or time)
  - Line 3 (Y=16): Timer countdown (mm:ss) + Heater temp
  - Line 4 (Y=24): Box temp + humidity + target temp
- **Does NOT**: Make operational decisions, store state, know business logic

#### **MenuController**
- Isolated menu state machine
- Handles navigation: `handleAction(ENTER, UP, DOWN, SAVE, CANCEL)`
- Maintains menu tree structure and current position
- **Handles value editing** for custom preset parameters and timer adjustment
- Fires `MenuSelectionCallback` when user confirms choice or value
- **Timer adjustment feature**:
  - Exposed in root menu as "Adjust Timer"
  - Allows ±10 minute increments
  - Rounds to nearest 10 minutes
  - Clamped to MIN_TIME_SECONDS / MAX_TIME_SECONDS
  - Updates via `setRemainingTime(seconds)` from Dryer stats
- Returns current menu items for display rendering
- Menu navigation uses history stack for "Back" functionality
- **Does NOT**: Execute actions, persist data, know about Dryer internals

#### **ButtonManager**
- Wraps mathertel/OneButton library
- Three buttons: SET, UP, DOWN
- Events: single click, long press
- Timing constants from Config.h: `BUTTON_DEBOUNCE_MS`, `BUTTON_CLICK_MS`, `BUTTON_LONG_PRESS_MS`
- Fires button-specific callbacks to whoever registers (UIController)
- **Does NOT**: Interpret meaning of button presses, know about menu or UI

#### **SoundController**
- Simple tone player (buzzer)
- Methods: `playClick()`, `playConfirm()`, `playStart()`, `playFinished()`, `playAlarm()`
- Can be enabled/disabled: `setEnabled(bool)`
- Triggered via direct calls from UIController, SafetyMonitor, Dryer
- **Does NOT**: Decide when to play sounds
- **NOTE**: Interface defined but implementation not yet created (TODO item)

#### **SettingsStorage**
- Pure persistence layer - no business logic
- LittleFS file operations
- JSON serialization/deserialization (ArduinoJson)
- Two files (paths defined in Config.h):
  - Settings file - user preferences, custom preset, sound on/off
  - Runtime file - current run state for power loss recovery
- Methods: `saveSettings()`, `loadSettings()`, `saveRuntimeState()`, `loadRuntimeState()`, `clearRuntimeState()`, `saveCustomPreset()`, `loadCustomPreset()`
- Loads ANY state from file - does not filter or validate
- **Does NOT**: Validate settings, make policy decisions, decide which states are recoverable (Dryer does this)

### 4. Data Flow Patterns

#### Sensor Reading Flow
```
SensorManager.update(currentMillis)
  ├─> DS18B20 async read (HEATER_TEMP_INTERVAL)
  │     ├─> requestConversion() → wait → isConversionReady() → read()
  │     └─> cache → callback(heaterTemp, timestamp)
  │                    ├─> SafetyMonitor.notifyHeaterTemp()
  │                    ├─> PIDController.compute()
  │                    │     └─> (with temp-aware slowdown + predictive cooling)
  │                    │         └─> Returns PWM (capped to PWM_MAX_PID_OUTPUT)
  │                    │             └─> Dryer → HeaterControl.setPWM()
  │                    └─> Display (via pull on refresh)
  │
  └─> AM2320 read (BOX_DATA_INTERVAL) → cache → callback(boxTemp, humidity, timestamp)
                                          ├─> SafetyMonitor.notifyBoxTemp()
                                          └─> Display (via pull on refresh)
```

#### User Input Flow (Menu Navigation)
```
Button Press (hardware)
  └─> ButtonManager (OneButton event)
      └─> ButtonCallback (registered by UIController)
          └─> UIController translates to MenuAction
              └─> MenuController.handleAction(ENTER|UP|DOWN)
                  ├─> Updates menu state
                  ├─> If in edit mode: adjust value
                  └─> If confirming: MenuSelectionCallback
                      └─> UIController.handleMenuSelection(path, value)
                          └─> Calls appropriate Dryer method
                              └─> Dryer updates internal state
                                  └─> StatsUpdateCallback → Display
```

#### Timer Adjustment Flow
```
User enters "Adjust Timer" menu item
  └─> MenuController shows current remaining time (rounded to 10min)
      └─> User adjusts with UP/DOWN (±10 min increments)
          └─> User presses SET to confirm
              └─> MenuSelectionCallback(ADJUST_TIMER, newMinutes)
                  └─> UIController calculates delta: (newValue - current)
                      └─> Dryer.adjustRemainingTime(deltaSeconds)
                          ├─> Clamps to MIN_TIME_SECONDS / MAX_TIME_SECONDS
                          ├─> Updates targetTimeSeconds
                          └─> StatsUpdateCallback → MenuController.setRemainingTime()
```

#### Preset Change During Run Flow
```
User changes preset while RUNNING or PAUSED
  └─> Dryer.selectPreset(newPreset)
      ├─> Loads new preset (temp, time, overshoot)
      ├─> Updates PIDController.setMaxAllowedTemp()
      ├─> Updates SafetyMonitor limits
      ├─> Resets timer: startTime = currentTime
      ├─> Resets totalPausedDuration = 0
      └─> If PAUSED: also updates pausedTime = currentTime
```

#### Emergency Flow
```
SensorManager detects overheat or SafetyMonitor timeout
  └─> SafetyMonitor.notifyHeaterTemp(95°C) or update() timeout
      └─> EmergencyStopCallback
          └─> Dryer emergency handler
              ├─> HeaterControl.emergencyStop()
              ├─> FanControl.stop() (if present)
              ├─> Dryer.transitionToState(FAILED)
              ├─> SoundController.playAlarm()
              └─> SettingsStorage.saveEmergencyState()
```

### 5. State Management

#### Dryer States
- **READY**: Idle, awaiting user start command
- **RUNNING**: Active drying cycle
- **PAUSED**: User paused, can resume
- **FINISHED**: Target time reached successfully
- **FAILED**: Safety violation or error
- **POWER_RECOVERED**: Recovered from power loss, awaiting user action

#### State Persistence
- Save to LittleFS at interval defined by `STATE_SAVE_INTERVAL` during RUNNING and when entering PAUSED
- Include: state, elapsed time, target temp/time, active preset, timestamp
- On boot:
  1. SettingsStorage loads whatever state was saved from file
  2. Dryer checks if state is recoverable (RUNNING or PAUSED)
  3. If recoverable: transition to POWER_RECOVERED with preserved timing
  4. If not recoverable: normal startup to READY
- User must explicitly continue (via `start()`) or reset
- **Separation of Concerns**: Storage is a dumb persistence layer; Dryer contains business logic for recovery validation

### 6. Timing Specifications

All timing intervals are defined in Config.h and should be referenced by constant name:

| Component | Interval Constant | Purpose |
|-----------|------------------|---------|
| Dryer.update() | - | Called every loop iteration |
| SensorManager (heater) | `HEATER_TEMP_INTERVAL` | DS18B20 async conversion + read cycle |
| SensorManager (box) | `BOX_DATA_INTERVAL` | AM2320 reading interval |
| PID compute | `PID_UPDATE_INTERVAL` | Triggered by heater temp callback |
| HeaterControl PWM period | `HEATER_PWM_PERIOD_MS` | Software PWM cycle duration |
| HeaterControl.update() | - | Must be called every loop for PWM timing |
| State persistence | `STATE_SAVE_INTERVAL` | Only during RUNNING |
| Display refresh | `DISPLAY_UPDATE_INTERVAL` | Pull current stats |
| Safety timeout | `SENSOR_TIMEOUT` | Max time between sensor readings |

**Critical**: HeaterControl requires frequent `update()` calls (ideally < 100ms) to maintain accurate software PWM timing.

### 7. Safety Architecture

#### Defense in Depth
1. **PID temperature-aware slowdown**: Primary mechanism - scales output within `PID_TEMP_SLOWDOWN_MARGIN` of max temp
2. **PID predictive cooling**: Prevents undershoot by anticipating temperature drop during cooldown
3. **PID output limit**: Capped to `PWM_MAX_PID_OUTPUT` (e.g., 50% of full scale) to prevent thermal momentum overshoot
4. **PID emergency stop**: Zero output if at or above max allowed temp
5. **SafetyMonitor**: Software watchdog with emergency stop callback
6. **Hardware bimetal switch**: Physical temperature cutoff on main power (outside software)

#### Safety Limits
All limits defined in Config.h:
- Max heater temp: `MAX_HEATER_TEMP`
- Max box temp: `MAX_BOX_TEMP`
- Sensor timeout: `SENSOR_TIMEOUT`
- Max drying time: `MAX_TIME_SECONDS`
- PID output limit: `PWM_MAX_PID_OUTPUT`

### 8. File Organization

```
project_root/
├── platformio.ini                    # PlatformIO configuration (3 environments)
├── specification.MD                  # This file
│
├── src/
│   ├── main.cpp                      # Setup, loop, serial command handler
│   ├── Dryer.h                       # Main orchestrator
│   ├── Types.h                       # Shared structs/enums/typedefs
│   ├── Config.h                      # Pin definitions, constants (all timing/limits)
│   ├── ComponentFactory.h            # Production/Mock factories
│   │
│   ├── interfaces/
│   │   ├── IBoxTempHumiditySensor.h
│   │   ├── IButtonManager.h
│   │   ├── IDisplay.h
│   │   ├── IDryer.h
│   │   ├── IFanControl.h
│   │   ├── IHeaterControl.h
│   │   ├── IHeaterTempSensor.h
│   │   ├── IMenuController.h
│   │   ├── IPIDController.h
│   │   ├── ISafetyMonitor.h
│   │   ├── ISensorManager.h
│   │   ├── ISettingsStorage.h
│   │   └── ISoundController.h
│   │
│   ├── sensors/
│   │   ├── SensorManager.h           # Multi-sensor coordinator with async reads
│   │   ├── HeaterTempSensor.h        # DS18B20 wrapper (async pattern)
│   │   └── BoxTempHumiditySensor.h   # AM2320 wrapper
│   │
│   ├── control/
│   │   ├── HeaterControl.h           # Software PWM controller
│   │   ├── PIDController.h           # PID with anti-windup, predictive cooling
│   │   ├── SafetyMonitor.h           # Safety watchdog
│   │   └── FanControl.h              # Simple fan relay control
│   │
│   ├── storage/
│   │   └── SettingsStorage.h         # LittleFS + JSON persistence
│   │
│   └── userInterface/
│       ├── UIController.h            # UI coordinator with dirty flag optimization
│       ├── MenuController.h          # Menu state machine with timer adjustment
│       ├── ButtonManager.h           # Button event handling
│       ├── OLEDDisplay.h             # Display driver
│       └── SoundController.h         # Buzzer/tone player
│
└── test/
    ├── README
    │
    ├── mocks/
    │   ├── arduino_mock.h
    │   ├── MockBoxTempHumiditySensor.h
    │   ├── MockDisplay.h
    │   ├── MockDryer.h
    │   ├── MockFanControl.h
    │   ├── MockHeaterControl.h
    │   ├── MockHeaterTempSensor.h
    │   ├── MockPIDController.h
    │   ├── MockSafetyMonitor.h
    │   ├── MockSensorManager.h
    │   ├── MockSettingsStorage.h
    │   └── MockSoundController.h
    │
    ├── test_display/
    │   └── test_display.cpp
    ├── test_dryer_integration/
    │   └── test_dryer_integration.cpp
    ├── test_fan_control/
    │   └── test_fan_control.cpp
    ├── test_heater_control/
    │   └── test_heater_control.cpp
    ├── test_pid_controller/
    │   └── test_pid_controller.cpp
    ├── test_safety_monitor/
    │   └── test_safety_monitor.cpp
    └── test_sensor_integration/
        └── test_sensor_integration.cpp
```

### 9. Coding Conventions

#### Naming
- **Interfaces**: Prefix with `I` (e.g., `ISensorManager`)
- **Callbacks**: Suffix with `Callback` (e.g., `StateChangeCallback`)
- **Constants**: `UPPER_SNAKE_CASE` (all defined in Config.h)
- **Private members**: camelCase with no prefix
- **Methods**: camelCase
- **Enums**: PascalCase for enum class, UPPER_SNAKE_CASE for values

### 10. Testing Guidelines

#### Unit Test Structure
- Arrange-Act-Assert pattern
- Use mocks for all dependencies
- Time injection for deterministic tests
- No hardware dependencies in tests
- Test files named `test_<component>/test_<component>.cpp`

#### Mock Components
- Implement same interface as production
- Allow setting return values
- Track method calls for verification
- Store in `test/mocks/` directory

#### Time-Dependent Tests
- Always pass time as parameter
- Advance time manually in tests
- Never use `millis()` or `delay()` in test code

### 11. Configuration

All configuration values are centralized in `Config.h`. The specification references constants by name, not by value, to maintain single source of truth.

#### Hardware Configuration
- Board selection via `#define ESP32_C3_BOARD` or `#define ESP32_S3_BOARD`
- Pin assignments for each board
- I2C configuration

#### Operational Limits
- Temperature ranges (MIN_TEMP, MAX_BOX_TEMP, MAX_HEATER_TEMP)
- Time limits (MIN_TIME_SECONDS, MAX_TIME_SECONDS)
- PWM limits (PWM_MIN, PWM_MAX, PWM_MAX_PID_OUTPUT)
- Overshoot margins

#### Timing Intervals
- Sensor reading rates
- PID update interval
- State save interval
- Display refresh interval
- Button timing
- Timeout values

#### PID Configuration
- Three tuning profiles (SOFT, NORMAL, STRONG)
- Derivative filter coefficient
- Temperature slowdown margin
- Predictive cooling parameters

#### Preset Defaults
- PLA, PETG, ABS, and Custom presets
- Temperature, time, and overshoot for each

### 12. Dependencies

#### Required Libraries
- **mathertel/OneButton**: Button handling
- **ArduinoJson**: Settings serialization
- **LittleFS**: File system
- **Wire**: I2C communication
- **OneWire + DallasTemperature**: DS18B20
- **Adafruit_AM2320**: Temperature/humidity sensor
- **Adafruit_SSD1306**: OLED display
- **Adafruit_GFX**: Graphics library

#### Unity PIO Testing
- Unity test framework
- No Arduino libraries in unit tests (mock all hardware)

### 13. Error Handling

#### Sensor Failures
- SensorManager detects invalid readings
- Fires `SensorErrorCallback`
- SafetyMonitor receives error and triggers emergency stop (if timeout exceeded)
- System transitions to FAILED state

#### Safety Violations
- SafetyMonitor detects limit breach
- Fires `EmergencyStopCallback`
- Dryer immediately stops heater and fan
- Transitions to FAILED state
- Saves emergency state to storage
- Plays alarm sound

#### Power Loss
- Runtime state saved at `STATE_SAVE_INTERVAL` intervals
- On boot, check for recent state file
- If valid (< `POWER_RECOVERY_TIMEOUT`), load and transition to POWER_RECOVERED
- Display shows recovery information
- User must acknowledge before continuing

---

## Quick Reference Checklist

When implementing a new component:

- [ ] Create interface with `I` prefix in `interfaces/` folder
- [ ] Pure virtual methods with `= 0`
- [ ] Virtual destructor `= default`
- [ ] Single `.h` file (no `.cpp`)
- [ ] Constructor takes dependencies as pointers
- [ ] `update(uint32_t currentMillis)` method if time-dependent
- [ ] **For HeaterControl specifically**: Must call `update()` frequently (< 100ms) for software PWM
- [ ] Use `std::function<>` for callbacks
- [ ] Provide `register*Callback()` methods
- [ ] Store callbacks in `std::vector<>`
- [ ] Check `if (callback)` before invoking
- [ ] Create production and mock factories
- [ ] Write unit tests with time injection
- [ ] Document responsibilities in header comment
- [ ] Place implementation in appropriate folder (sensors/, control/, userInterface/, storage/)
- [ ] Reference Config.h constants by name, never hardcode values
- [ ] For fan control: Optional dependency (nullptr check in consuming code)

---

## Component Template

```cpp
// interfaces/IComponentName.h
#ifndef I_COMPONENT_NAME_H
#define I_COMPONENT_NAME_H

#include <functional>

using ComponentCallback = std::function<void(int value)>;

/**
 * Interface for ComponentName
 * Responsibilities:
 * - Does X
 * - Does Y
 * Does NOT:
 * - Does not do Z
 */
class IComponentName {
public:
    virtual void update(uint32_t currentMillis) = 0;
    virtual void doSomething(int param) = 0;
    virtual int getState() const = 0;
    virtual void registerCallback(ComponentCallback callback) = 0;
    virtual ~IComponentName() = default;
};

#endif
```

```cpp
// control/ComponentName.h (or sensors/, userInterface/, storage/)
#ifndef COMPONENT_NAME_H
#define COMPONENT_NAME_H

#include "interfaces/IComponentName.h"
#include "Config.h"
#include <vector>

class ComponentName : public IComponentName {
private:
    int state;
    std::vector<ComponentCallback> callbacks;
    
    void notifyCallbacks(int value) {
        for (auto& callback : callbacks) {
            if (callback) callback(value);
        }
    }
    
public:
    ComponentName() : state(0) {}
    
    void update(uint32_t currentMillis) override {
        // Implementation using Config.h constants
    }
    
    void doSomething(int param) override {
        state = param;
        notifyCallbacks(state);
    }
    
    int getState() const override {
        return state;
    }
    
    void registerCallback(ComponentCallback callback) override {
        callbacks.push_back(callback);
    }
};

#endif
```

---

## Menu Tree Structure

```
HOME screen - showing current state and temps
  - Long press SET - pause/resume
  - Short press UP/DOWN - toggle between stats screens (BOX_TEMP, REMAINING, HEATER_TEMP)
  - Short press SET - enters main menu
    - Status
      - Start/resume (if READY/PAUSED)
      - Pause (if RUNNING)
      - Ready (if not READY)
      - Back
    - Select Preset (selecting immediately applies and exits menu)
      - PLA (50°C, 5h, 19°C overshoot)
      - PETG (65°C, 5h, 10°C overshoot)
      - Custom (user-defined values)
      - Back
    - Edit Custom preset (edits custom preset values, does NOT select it)
      - Temp (current value) - ±1°C
      - Time (current value) - ±10 min
      - Max Overshoot (current value) - ±1°C
      - Copy from PLA (copies PLA values to custom preset)
      - Back
    - Adjust Timer (current remaining time rounded to 10min)
      - Value edit: ±10 min increments
      - Clamped to MIN_TIME_SECONDS / MAX_TIME_SECONDS
    - PID (current profile name)
      - SOFT
      - NORMAL
      - STRONG
      - Back
    - Sound: On/Off (current value)
      - Toggle and save
    - System Info
      - Scrollable list of Config.h constants with values
      - Back
```

**Menu Navigation**:
- UP/DOWN: Navigate items (endless loop)
- SET: Enter/confirm
- LONG PRESS SET: Go back one level
- In edit mode: UP/DOWN adjust value, SET confirms

---

## PID Controller Detailed Specifications

### Anti-Windup Implementation
```cpp
// Integral term with anti-windup
float proposedIntegral = integral + ki * error * dtSec;

// Calculate output with proposed integral
float proposedOutput = pTerm + proposedIntegral + dTerm;

// Check if output would saturate
if (proposedOutput > outMax && error > 0) {
    // Don't accumulate if maxed out with positive error
    proposedIntegral = integral;
} else if (proposedOutput < outMin && error < 0) {
    // Don't accumulate if at min with negative error
    proposedIntegral = integral;
}

integral = proposedIntegral;
integral = constrain(integral, outMin, outMax);
```

### Derivative Smoothing (Low-Pass Filter)
```cpp
// Calculate raw derivative (on measurement, not error)
float dInput = (input - lastInput) / dtSec;
float rawDerivative = -kd * dInput;  // Negative to oppose change

// Apply exponential moving average
// filtered[n] = α * raw[n] + (1-α) * filtered[n-1]
filteredDerivative = PID_DERIVATIVE_FILTER_ALPHA * rawDerivative 
                   + (1.0 - PID_DERIVATIVE_FILTER_ALPHA) * filteredDerivative;
```

### Temperature-Aware Slowdown
```cpp
// Normal PID calculation
float output = pTerm + integral + dTerm;
output = constrain(output, outMin, outMax);

// Temperature-aware slowdown
float tempMargin = maxAllowedTemp - input;

if (input >= maxAllowedTemp) {
    // Emergency: stop completely
    output = 0.0;
    integral = 0.0;
} else if (tempMargin < PID_TEMP_SLOWDOWN_MARGIN) {
    // Approaching max: scale output proportionally
    float scaleFactor = tempMargin / PID_TEMP_SLOWDOWN_MARGIN;
    output *= scaleFactor;
    integral *= scaleFactor;  // Also scale integral to prevent windup
}
```

### Predictive Cooling Compensation (NEW)
```cpp
// Track cooling rate with exponential moving average
float rawCoolingRate = (input - lastInput) / dtSec;  // °C/s
coolingRate = COOLING_RATE_FILTER_ALPHA * rawCoolingRate
            + (1.0 - COOLING_RATE_FILTER_ALPHA) * coolingRate;

// If cooling faster than threshold, predict future temperature
if (coolingRate < MIN_COOLING_RATE) {  // e.g., -0.08 °C/s
    float predictedTemp = input + (coolingRate * PREDICTIVE_HORIZON_SEC);
    float predictedError = setpoint - predictedTemp;
    
    // If prediction shows undershoot, enhance error now
    if (predictedError > error) {
        error = error + (predictedError - error) * PREDICTIVE_GAIN;
    }
}

// Use enhanced error for proportional term
float pTerm = kp * error;
```

**Constants** (all defined in Config.h):
- `COOLING_RATE_FILTER_ALPHA`: Filter coefficient for cooling rate smoothing (e.g., 0.7)
- `MIN_COOLING_RATE`: Threshold to trigger prediction (e.g., -0.08 °C/s)
- `PREDICTIVE_HORIZON_SEC`: How far ahead to predict (e.g., 10 seconds)
- `PREDICTIVE_GAIN`: Amplification factor for predicted error (e.g., 1.5)

**Purpose**: Prevents temperature undershoot during cooldown by anticipating the thermal inertia of the system. When the temperature is dropping rapidly (cooling faster than threshold), the algorithm predicts where it will be in the near future and increases the heating response accordingly.

---

## Software PWM Implementation Details

HeaterControl uses software timing instead of hardware LEDC to achieve long PWM periods suitable for SSR relay longevity.

### Implementation Pattern
```cpp
class HeaterControl : public IHeaterControl {
private:
    uint32_t cycleStartTime;
    bool pinState;
    uint8_t currentPWM;
    
public:
    void update(uint32_t currentMillis) override {
        if (!running) return;
        
        // Calculate position in PWM cycle
        uint32_t elapsed = currentMillis - cycleStartTime;
        
        // Reset cycle if period elapsed
        if (elapsed >= HEATER_PWM_PERIOD_MS) {
            cycleStartTime = currentMillis;
            elapsed = 0;
        }
        
        // Calculate ON time for this cycle
        uint32_t onTimeMs = (HEATER_PWM_PERIOD_MS * currentPWM) / PWM_MAX;
        
        // Determine desired pin state
        bool shouldBeHigh = (elapsed < onTimeMs);
        
        // Only update GPIO if state needs to change
        if (shouldBeHigh != pinState) {
            pinState = shouldBeHigh;
            digitalWrite(pwmPin, pinState ? HIGH : LOW);
        }
    }
};
```

**Critical Requirements**:
- `update()` must be called frequently (< 100ms intervals) for accurate timing
- PWM period defined by `HEATER_PWM_PERIOD_MS` (typically 2000-5000ms for SSR)
- Duty cycle: 0-100 mapped to 0-`PWM_MAX` from Config.h
- Pin state only changes when necessary (not every update)

**Advantages over hardware PWM**:
- Can achieve very long periods (>1 second) unsuitable for hardware LEDC
- No conflict with other PWM channels
- Easy to debug and verify timing

**Disadvantages**:
- Requires frequent loop execution
- Not suitable for high-frequency PWM
- Timing accuracy depends on loop consistency

---

## Display Layout Specifications

### HOME Mode - Stats Screens

**Screen 1: BOX_TEMP (Default)**
```
Line 1.1 (Y=0):  [State] __________ [Preset]
                  R/>/|/F/!/P       PLA/PETG/CUST
Line 1.2 (Y=8):  B:
Lines 1.2+2:      [Box Temp]°C
                  (size 2 font)
Line 3 (Y=16):   [mm:ss] _____ H:[Heater]°C
Line 4 (Y=24):   B:[Box]°C [Hum]% /[Target]°C
```

**Screen 2: REMAINING**
```
Line 1.1 (Y=0):  [State] __________ [Preset]
Lines 1.2+2:      [h:mm:ss]
                  (size 2 font, remaining time)
Line 3 (Y=16):   [mm:ss] _____ H:[Heater]°C
Line 4 (Y=24):   B:[Box]°C [Hum]% /[Target]°C
```

**Screen 3: HEATER_TEMP**
```
Line 1.1 (Y=0):  [State] __________ [Preset]
Line 1.2 (Y=8):  H:
Lines 1.2+2:      [Heater Temp]°C
                  (size 2 font)
Line 3 (Y=16):   [mm:ss] _____ H:[Heater]°C
Line 4 (Y=24):   B:[Box]°C [Hum]% /[Target]°C
```

**State Characters**:
- `R` = READY
- `>` = RUNNING
- `|` = PAUSED
- `F` = FINISHED
- `!` = FAILED
- `P` = POWER_RECOVERED

**Preset Abbreviations**:
- `PLA` = PLA preset
- `PETG` = PETG preset
- `CUST` = Custom preset

### MENU Mode - Navigation

**Submenu List (Left-aligned, endless loop)**
```
prevItem      (font size 1)
currentItem   (font size 2, highlighted)
nextItem      (font size 1)
```

**Value Edit (Center-aligned)**
```
Item label    (font size 1)
[4px spacing]
currentValue + unit (font size 2)
```

**System Info List (Left-aligned)**
```
Item label    (font size 1)
[4px spacing]
Item value    (font size 2)
```

---

## Timer Adjustment Feature

### User Flow
1. User navigates to "Adjust Timer" in root menu
2. MenuController displays current remaining time rounded to nearest 10 minutes
3. User adjusts with UP/DOWN buttons (±10 minute increments)
4. Value is visually clamped to `MIN_TIME_SECONDS` / `MAX_TIME_SECONDS` (converted to minutes for display)
5. User presses SET to confirm
6. MenuSelectionCallback fires with `MenuPath::ADJUST_TIMER` and new value in minutes
7. UIController receives callback, calculates delta from current remaining time
8. Calls `Dryer.adjustRemainingTime(deltaSeconds)`
9. Dryer clamps the result and updates `targetTimeSeconds`
10. Stats update callback refreshes MenuController with new remaining time

### Implementation Notes
- Timer adjustment works in any state (READY, RUNNING, PAUSED)
- When RUNNING: Changes take effect immediately, affects when FINISHED state is reached
- When PAUSED: Changes take effect when resumed
- When READY: Sets the target time for when start is pressed
- Minimum adjustment: 10 minutes
- Values rounded to nearest 10 minutes for UI simplicity
- Internal timing remains precise (second resolution)

### Dryer Method
```cpp
void adjustRemainingTime(int32_t deltaSeconds) override {
    // Calculate new target time
    int32_t newTargetTime = (int32_t)targetTimeSeconds + deltaSeconds;
    
    // Clamp to valid range
    newTargetTime = constrain(newTargetTime, 
                              MIN_TIME_SECONDS, 
                              MAX_TIME_SECONDS);
    
    targetTimeSeconds = (uint32_t)newTargetTime;
}
```

---

## Preset Change During Operation

### Behavior (NEW)
Previously, preset changes were not allowed during RUNNING or PAUSED states. The current implementation now **allows preset changes during operation** with automatic timer reset.

### Timer Reset Logic
When preset is changed during RUNNING or PAUSED:
1. New preset loaded (target temp, target time, max overshoot)
2. PID constraints updated
3. Safety monitor limits updated
4. **Timer reset**:
  - `startTime = currentMillis`
  - `totalPausedDuration = 0`
  - If PAUSED: `pausedTime = currentMillis`
5. Elapsed time effectively becomes 0
6. New target time from new preset takes effect

### Use Case
Allows user to switch filament types mid-dry without stopping and restarting. For example:
- Started with PLA preset (50°C, 5 hours)
- After 2 hours, switch to PETG (65°C, 5 hours)
- Timer resets, will now run for full 5 hours at 65°C
- No need to stop, reset, and restart

### UI Flow
```
User in RUNNING state with PLA preset
  └─> Navigates to "Select Preset" menu
      └─> Selects "PETG"
          └─> MenuSelectionCallback(PRESET_PETG, 0)
              └─> UIController → Dryer.selectPreset(PresetType::PETG)
                  ├─> Loads PETG parameters
                  ├─> Updates PID max temp (65 + 10 = 75°C)
                  ├─> Resets startTime = currentMillis
                  ├─> totalPausedDuration = 0
                  └─> State remains RUNNING
                      └─> StatsUpdateCallback shows new preset and reset timer
```

---

## Fan Control Integration

### Purpose
Optional cooling fan to ensure proper air circulation during heating and cooldown.

### Operating Rules
- **Starts**: When Dryer transitions to RUNNING
- **Keeps Running**: When Dryer transitions to PAUSED (for cooling)
- **Stops**: When Dryer transitions to READY, FINISHED, or FAILED
- **Emergency**: Stops when SafetyMonitor triggers emergency

### State Transition Behavior
```
READY → RUNNING:     Fan starts
RUNNING → PAUSED:    Fan keeps running (does NOT stop)
PAUSED → RUNNING:    Fan already running (no action)
PAUSED → READY:      Fan stops
RUNNING → FINISHED:  Fan stops
RUNNING → FAILED:    Fan stops
Any → FAILED:        Fan stops (emergency)
```

### Implementation
Fan control is **optional** - Dryer accepts `nullptr` for fan control pointer and checks before calling methods:

```cpp
// In Dryer state transitions
if (fanControl) {
    fanControl->start();  // or stop()
}

// Null safety pattern used throughout
if (fanControl && !fanControl->isRunning()) {
    fanControl->start();
}
```

### Interface
```cpp
class IFanControl {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual ~IFanControl() = default;
};
```

Simple relay control (ON/OFF), no PWM or speed control.

---

## Dirty Flag Optimization (UIController)

### Problem
Updating the display on every loop iteration is wasteful and can cause flicker, especially when values haven't changed.

### Solution
UIController maintains a `displayNeedsUpdate` flag that is set `true` only when:
1. Button press occurs
2. Menu navigation happens
3. Displayed values actually change (HOME mode only)

### Implementation Pattern
```cpp
class UIController {
private:
    bool displayNeedsUpdate;
    
    // Cached values for change detection
    struct CachedDisplayValues {
        float boxTemp;
        float heaterTemp;
        float boxHumidity;
        uint32_t remainingTime;
        DryerState state;
        PresetType preset;
        
        bool operator!=(const CachedDisplayValues& other) const {
            // Return true if any significant change detected
        }
    };
    CachedDisplayValues cachedValues;
    
public:
    void update(uint32_t currentMillis) {
        // Store time for callback consistency
        currentTime = currentMillis;
        
        // Update buttons (high priority)
        buttonManager->update(currentMillis);
        
        // Check menu timeout
        checkMenuTimeout(currentMillis);
        
        // Only render if dirty
        if (displayNeedsUpdate) {
            if (currentMode == UIMode::HOME) {
                renderHomeScreen();
            } else {
                renderMenuScreen();
            }
            displayNeedsUpdate = false;  // Clear flag
        }
    }
};
```

### When Flag is Set
**In HOME mode**:
- Stats callback from Dryer detects value change (temperature, time, state, preset)
- User presses button (cycles stats screens)
- User enters menu

**In MENU mode**:
- Any button press (navigation, edit)
- Menu state changes
- User exits menu

### Benefits
- Reduces SPI/I2C traffic to display
- Prevents flicker from redundant updates
- Lower power consumption
- Faster loop execution when display unchanged

---

## Current Time Storage Pattern (UIController)

### Problem
Callbacks may fire at different times during the update cycle, leading to timing inconsistencies when using `millis()` directly.

### Solution
UIController stores `currentMillis` at the start of `update()` and uses this stored value in all callbacks and logic during that update cycle.

### Implementation
```cpp
class UIController {
private:
    uint32_t currentTime;  // Stored at start of update()
    
public:
    void update(uint32_t currentMillis) {
        // Store time for consistency
        currentTime = currentMillis;
        
        // Now all callbacks during this update use the same time
        buttonManager->update(currentMillis);
        
        // Button callbacks use currentTime, not millis()
        // Menu timeout check uses currentTime
        // All timing logic consistent within this update cycle
    }
};
```

### Benefits
- Deterministic timing within an update cycle
- No race conditions from time advancing mid-update
- Easier to test with fixed time injection
- Consistent behavior across callbacks

---

## Critical Reminders

### HeaterControl
- **Must call `update(currentMillis)` frequently** (< 100ms) from main loop
- Software PWM timing depends on loop consistency
- Pin state changes only when necessary for efficiency
- Period configured via `HEATER_PWM_PERIOD_MS` in Config.h

### PIDController
- Output is capped to `PWM_MAX_PID_OUTPUT`, not `PWM_MAX`
- This is the primary safety limit to prevent overshoot from thermal momentum
- Temperature-aware slowdown is the primary overshoot prevention mechanism
- Predictive cooling prevents undershoot during temperature drop
- Always use Config.h constants, never hardcode tuning values

### Configuration
- **All timing, limits, and tuning values in Config.h**
- Specification references constants by name only
- Single source of truth for all operational parameters
- Never replicate values in code or documentation

### Safety
- Defense in depth: multiple layers (PID output limit, temp slowdown, safety monitor, hardware switch)
- Fan keeps running when paused for cooling
- Emergency stops both heater and fan immediately
- State persistence allows recovery from power loss

### Testing
- All components have corresponding test files
- Mocks for all hardware dependencies
- Time injection for deterministic tests
- Integration tests verify component interactions

---

## Summary

This architecture provides:
- **Testability**: Interface-based design with dependency injection and time injection
- **Safety**: Multiple layers of protection with defense in depth
- **Flexibility**: Callback-based communication enables loose coupling
- **Maintainability**: Clear component responsibilities and single source of truth for configuration
- **Recoverability**: State persistence enables recovery from power loss
- **User Experience**: Dirty flag optimization, timer adjustment, preset change during operation
- **Predictability**: Predictive cooling compensation prevents temperature undershoot

All timing constants, safety limits, and tuning parameters are centralized in `Config.h` and referenced by name throughout the codebase to maintain a single source of truth.