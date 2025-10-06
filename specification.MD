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
- Persists runtime state every second
- **Does NOT**: Read sensors directly, control hardware directly, handle UI logic, validate constraints

#### **SensorManager**
- Owns all physical sensors (DS18B20, AM2320)
- Multi-rate reading strategy:
    - Heater temp (DS18B20): 500ms interval
    - Box temp/humidity (AM2320): 2000ms interval
- Maintains cached readings with timestamps and validity flags
- **Push interface**: Callbacks on new readings
    - `registerHeaterTempCallback(callback)` - fires every 500ms
    - `registerBoxDataCallback(callback)` - fires every 2000ms
    - `registerSensorErrorCallback(callback)` - fires on failures
- **Pull interface**: Returns cached values on demand
    - `getReadings()`, `getHeaterTemp()`, `getBoxTemp()`, `getBoxHumidity()`
- Detects and reports sensor failures
- **Does NOT**: Process or interpret readings, enforce limits

#### **PIDController**
- **Stateful** - maintains integral accumulation and last input for derivative
- Subscribes to heater temp updates (500ms) via callback
- Computes PWM output: `compute(setpoint, input, currentMillis)`
- **Three profiles**: `setProfile(PIDProfile::SOFT | NORMAL | STRONG)`
    - **SOFT**: Kp=2.0, Ki=0.5, Kd=1.0 - gentle heating, slower response
    - **NORMAL**: Kp=4.0, Ki=1.0, Kd=2.0 - balanced response
    - **STRONG**: Kp=6.0, Ki=1.5, Kd=3.0 - aggressive heating, faster response
- Output limits: `setLimits(outMin, outMax)`
- **Anti-windup protection**:
    - Clamp integral term to output limits before adding to total output
    - When output saturates (hits min/max), stop accumulating integral
    - Reset integral when error changes sign or system restarts
  ```cpp
  // Anti-windup implementation
  integral += ki * error * dtSec;
  
  // Clamp integral to output range
  integral = constrain(integral, outMin, outMax);
  
  float output = pTerm + integral + dTerm;
  
  // If output is saturated, prevent further integral accumulation
  if ((output >= outMax && error > 0) || (output <= outMin && error < 0)) {
      integral -= ki * error * dtSec;  // Undo this accumulation
  }
  ```
- **Derivative smoothing** (low-pass filter):
    - Use exponential moving average on derivative term to reduce noise sensitivity
    - Filter coefficient α = 0.2 (adjust for more/less smoothing)
  ```cpp
  float rawDerivative = -kd * dInput;
  
  // Low-pass filter: filtered = α * raw + (1-α) * lastFiltered
  const float alpha = 0.2;
  filteredDerivative = alpha * rawDerivative + (1.0 - alpha) * filteredDerivative;
  
  float dTerm = filteredDerivative;
  ```
- **Temperature-aware slowdown**:
    - When approaching max allowed temp (within 5°C), scale down entire PID output
    - This is the PRIMARY mechanism for preventing overshoot
  ```cpp
  float compute(float setpoint, float input, uint32_t currentMillis) {
      // Calculate PID output normally
      float output = pTerm + integral + dTerm;
      output = constrain(output, outMin, outMax);
      
      // Scale down when approaching max allowed temp
      float tempMargin = maxAllowedTemp - input;
      if (tempMargin < 5.0 && tempMargin > 0) {
          float scaleFactor = tempMargin / 5.0;  // 0-1 range
          output *= scaleFactor;
      }
      
      // Emergency stop if over max temp
      if (input >= maxAllowedTemp) {
          output = 0;
      }
      
      return output;
  }
  ```
- Derivative on measurement (not error) to avoid setpoint kick
- `reset()` method to clear state when starting/stopping
- **Does NOT**: Read sensors, control heater directly, but DOES enforce temperature slowdown

#### **HeaterControl**
- Controls heater PWM output (ESP32 LEDC)
- **Simple relay**: Directly applies PWM value from PID without modification
- Methods: `start()`, `stop()`, `emergencyStop()`, `setPWM()`, `isRunning()`
- PWM period: 5 seconds
- **Does NOT**: Scale or modify PWM values, make safety decisions, read sensors
- Trusts PID for proper control and SafetyMonitor for emergencies

#### **SafetyMonitor**
- **Passive guardian** - monitors but doesn't control
- Subscribes to all sensor updates via callbacks
- Checks hard limits:
    - Max heater temp: 90°C (80°C + 10°C overshoot)
    - Max box temp: 80°C
    - Sensor timeout: 5 seconds
- Triggers `EmergencyStopCallback` on violations
- Dryer's emergency callback stops heater and transitions to FAILED state
- **Hardware backup**: 80°C bimetal switch on main power
- **Does NOT**: Control heater directly, make operational decisions

#### **UIController**
- Owns display, menu controller, button manager
- **Registers button callbacks** with ButtonManager
- Translates button presses to MenuController actions
- Bridges menu selections to Dryer via callbacks
- Subscribes to Dryer stats updates for display
- Triggers sound feedback via SoundController
- Handles display refresh (pull stats on demand, typically ~200ms)
- **Button registration pattern**:
  ```cpp
  void UIController::setup() {
      // Register with ButtonManager
      buttonManager->registerButtonCallback(ButtonType::SET, 
          [this](ButtonEvent event) {
              if (event == ButtonEvent::SINGLE_CLICK) {
                  menuController->handleAction(MenuAction::ENTER);
                  soundController->playClick();
              }
          }
      );
      
      buttonManager->registerButtonCallback(ButtonType::UP,
          [this](ButtonEvent event) {
              if (event == ButtonEvent::SINGLE_CLICK) {
                  menuController->handleAction(MenuAction::UP);
                  soundController->playClick();
              }
          }
      );
      
      buttonManager->registerButtonCallback(ButtonType::DOWN,
          [this](ButtonEvent event) {
              if (event == ButtonEvent::SINGLE_CLICK) {
                  menuController->handleAction(MenuAction::DOWN);
                  soundController->playClick();
              }
          }
      );
  }
  ```
- **Does NOT**: Make operational decisions, store state, know business logic

#### **MenuController**
- Isolated menu state machine
- Handles navigation: `handleAction(ENTER, UP, DOWN, BACK)`
- Maintains menu tree structure and current position
- **Handles value editing** for custom preset parameters
- Fires `MenuSelectionCallback` when user confirms choice or value
- **Custom preset editing flow**:
  ```
  User enters Custom preset menu
    ├─> Navigate to "Temp" → ENTER → edit mode
    │     ├─> UP/DOWN adjusts value (±1°C)
    │     ├─> Values clamped by min/max temp from Dryer
    │     └─> ENTER confirms → MenuSelectionCallback(CUSTOM_TEMP, newValue)
    │                        └─> UIController receives callback
    │                            └─> Updates Dryer's custom preset temp
    │
    ├─> Navigate to "Time" → ENTER → edit mode
    │     ├─> UP/DOWN adjusts value (±10 min)
    │     ├─> Values clamped by max time from Dryer
    │     └─> ENTER confirms → MenuSelectionCallback(CUSTOM_TIME, newValue)
    │
    ├─> Navigate to "Max Overshoot" → ENTER → edit mode
    │     ├─> UP/DOWN adjusts value (±1°C)
    │     └─> ENTER confirms → MenuSelectionCallback(CUSTOM_OVERSHOOT, newValue)
    │
    └─> Navigate to "Save" → ENTER
          └─> MenuSelectionCallback(SAVE_CUSTOM_PRESET, 0)
              └─> UIController calls Dryer.saveCustomPreset()
                  └─> Dryer calls SettingsStorage.saveCustomPreset()
  ```
- **Menu item structure**:
  ```cpp
  struct MenuItem {
      String label;
      MenuItemType type;  // SUBMENU, VALUE_EDIT, ACTION
      MenuPath path;      // Unique identifier for callback
      
      // For VALUE_EDIT type
      int currentValue;
      int minValue;
      int maxValue;
      int step;
      String unit;  // "°C", "min", etc.
  };
  ```
- Returns current menu items for display rendering
- `reset()` to return to root menu
- **Does NOT**: Execute actions, persist data, know about Dryer internals

#### **ButtonManager**
- Wraps mathertel/OneButton library
- Three buttons: SET, UP, DOWN
- Events: single click, long press
- Fires button-specific callbacks to whoever registers (UIController)
- **Does NOT**: Interpret meaning of button presses, know about menu or UI

#### **SoundController**
- Simple tone player (buzzer)
- Methods: `playClick()`, `playConfirm()`, `playStart()`, `playFinished()`, `playAlarm()`
- Can be enabled/disabled: `setEnabled(bool)`
- Triggered via direct calls from UIController, SafetyMonitor, Dryer
- **Does NOT**: Decide when to play sounds

#### **SettingsStorage**
- LittleFS file operations
- JSON serialization/deserialization (ArduinoJson)
- Two files:
    - `/settings.json` - user preferences, custom preset, sound on/off
    - `/runtime.json` - current run state for power loss recovery
- Methods: `saveSettings()`, `loadSettings()`, `saveRuntimeState()`, `loadRuntimeState()`, `clearRuntimeState()`, `saveCustomPreset()`, `loadCustomPreset()`
- **Does NOT**: Validate settings, make policy decisions

### 4. Data Flow Patterns

#### Sensor Reading Flow
```
SensorManager.update(currentMillis)
  ├─> DS18B20 read (500ms) ─> cache ─> callback(heaterTemp, timestamp)
  │                                         ├─> SafetyMonitor.notifyHeaterTemp()
  │                                         ├─> PIDController.compute() 
  │                                         │     └─> (with temp-aware slowdown)
  │                                         │         └─> HeaterControl.setPWM()
  │                                         └─> Display (via pull on refresh)
  │
  └─> AM2320 read (2000ms) ─> cache ─> callback(boxTemp, humidity, timestamp)
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

#### Custom Preset Value Flow
```
User navigates to Custom > Temp > ENTER (enters edit mode)
  └─> MenuController enters VALUE_EDIT mode
      └─> Display shows: "Temp: 50°C" (current value, blinking)
      
User presses UP
  └─> ButtonManager → UIController → MenuController.handleAction(UP)
      └─> MenuController increments value (50 → 51)
          └─> Display updates: "Temp: 51°C"

User presses ENTER (confirms)
  └─> MenuController exits edit mode
      └─> MenuSelectionCallback(CUSTOM_TEMP, 51)
          └─> UIController receives callback
              └─> Calls Dryer.setCustomPresetTemp(51)
                  └─> Dryer updates customPreset.targetTemp = 51

User navigates to Custom > Save > ENTER
  └─> MenuSelectionCallback(SAVE_CUSTOM_PRESET, 0)
      └─> UIController calls Dryer.saveCustomPreset()
          └─> Dryer calls SettingsStorage.saveCustomPreset(customPreset)
              └─> Writes to /settings.json
```

#### Emergency Flow
```
SensorManager detects overheat
  └─> SafetyMonitor.notifyHeaterTemp(95°C)
      └─> EmergencyStopCallback
          └─> Dryer emergency handler
              ├─> HeaterControl.emergencyStop()
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
- Save to LittleFS every minute during RUNNING
- Include: state, elapsed time, target temp/time, active preset, timestamp
- On boot: check for valid runtime state
- If found and recent RUNNING state after power on: transition to POWER_RECOVERED
- User must explicitly continue or reset

### 6. Timing Specifications

| Component | Update Rate | Notes |
|-----------|-------------|-------|
| Dryer.update() | Every loop | Called from main loop() |
| SensorManager (heater) | 500ms | DS18B20 reading |
| SensorManager (box) | 2000ms | AM2320 reading |
| PID compute | 500ms | Triggered by heater temp callback |
| HeaterControl PWM period | 5000ms | LEDC period |
| State persistence | 1000ms | Only during RUNNING |
| Display refresh | ~200ms | Pull current stats |
| Safety check | Every sensor update + timeout check |

### 7. Safety Architecture

#### Defense in Depth
1. **PID temperature-aware slowdown**: Primary mechanism - scales output within 5°C of max temp
2. **PID emergency stop**: Zero output if at or above max allowed temp
3. **SafetyMonitor**: Software watchdog with emergency stop callback
4. **Hardware bimetal switch**: 80°C physical cutoff on main power

#### Safety Limits
- Max heater temp: 90°C (80°C target + 10°C overshoot)
- Max box temp: 80°C
- Sensor timeout: 5 seconds
- Max drying time: 10 hours

### 8. File Organization

```
src/
├── main.cpp                          # Setup and loop
├── Dryer.h                           # Main orchestrator
├── Types.h                           # Shared structs/enums/typedefs
├── Config.h                          # Pin definitions, constants
├── ComponentFactory.h                # Production/Mock factories
│
├── interfaces/
│   ├── ISensorManager.h
│   ├── IHeaterControl.h
│   ├── IPIDController.h
│   ├── ISafetyMonitor.h
│   ├── ISettingsStorage.h
│   ├── IDisplay.h
│   ├── IMenuController.h
│   └── ISoundController.h
│
├── sensors/
│   ├── SensorManager.h               # Multi-sensor coordinator
│   ├── DS18B20Sensor.h               # Heater temp sensor wrapper
│   └── AM2320Sensor.h                # Box temp/humidity wrapper
│
├── control/
│   ├── HeaterControl.h               # PWM heater control
│   ├── PIDController.h               # PID algorithm with anti-windup
│   └── SafetyMonitor.h               # Safety watchdog
│
├── storage/
│   └── SettingsStorage.h             # LittleFS + JSON persistence
│
└── userInterface/
    ├── UIController.h                # UI coordinator
    ├── MenuController.h              # Menu state machine
    ├── ButtonManager.h               # Button event handling
    ├── OLEDDisplay.h                 # Display driver
    └── SoundController.h             # Buzzer/tone player

test/
├── test_dryer/
├── test_sensor_manager/
├── test_pid_controller/
├── test_safety_monitor/
├── test_menu_controller/
└── mocks/
    ├── MockSensorManager.h
    ├── MockHeaterControl.h
    ├── MockPIDController.h
    └── ...
```

### 9. Coding Conventions

#### Naming
- **Interfaces**: Prefix with `I` (e.g., `ISensorManager`)
- **Callbacks**: Suffix with `Callback` (e.g., `StateChangeCallback`)
- **Constants**: `UPPER_SNAKE_CASE`
- **Private members**: camelCase with no prefix
- **Methods**: camelCase
- **Enums**: PascalCase for enum class, UPPER_SNAKE_CASE for values

#### Patterns
```cpp
// Callback type definition
using SensorUpdateCallback = std::function<void(float temp, uint32_t timestamp)>;

// Callback registration
void registerSensorCallback(SensorUpdateCallback callback);

// Callback storage
std::vector<SensorUpdateCallback> callbacks;

// Callback invocation
for (auto& callback : callbacks) {
    if (callback) callback(temp, millis());
}

// Interface definition
class ISensorManager {
public:
    virtual void update(uint32_t currentMillis) = 0;
    virtual float getHeaterTemp() const = 0;
    virtual ~ISensorManager() = default;
};

// Implementation
class SensorManager : public ISensorManager {
private:
    float cachedTemp;
public:
    void update(uint32_t currentMillis) override;
    float getHeaterTemp() const override { return cachedTemp; }
};
```

#### Factory Pattern
```cpp
class ISensorManagerFactory {
public:
    virtual ISensorManager* create() = 0;
    virtual ~ISensorManagerFactory() = default;
};

class ProductionSensorManagerFactory : public ISensorManagerFactory {
public:
    ISensorManager* create() override {
        return new SensorManager(HEATER_PIN, SDA_PIN, SCL_PIN);
    }
};

class MockSensorManagerFactory : public ISensorManagerFactory {
public:
    ISensorManager* create() override {
        return new MockSensorManager();
    }
};
```

### 10. Testing Guidelines

#### Unit Test Structure
```cpp
void test_pid_accumulates_integral() {
    // Arrange
    PIDController pid;
    pid.setProfile(PIDProfile::NORMAL);
    pid.setLimits(0, 255);
    
    // Act
    float output1 = pid.compute(50.0, 45.0, 0);
    float output2 = pid.compute(50.0, 45.0, 1000);
    
    // Assert
    TEST_ASSERT_TRUE(output2 > output1);
}

void test_pid_slows_near_max_temp() {
    // Arrange
    PIDController pid;
    pid.setProfile(PIDProfile::STRONG);
    pid.setMaxAllowedTemp(90.0);
    
    // Act - at 87°C (3°C from max)
    float output87 = pid.compute(90.0, 87.0, 0);
    
    // Act - at 50°C (40°C from max)
    float output50 = pid.compute(90.0, 50.0, 1000);
    
    // Assert - output at 87°C should be scaled down
    TEST_ASSERT_TRUE(output87 < output50 * 0.65);  // Scaled by ~0.6
}
```

#### Mock Components
- Implement same interface as production
- Allow setting return values
- Track method calls for verification
- No hardware dependencies

#### Time-Dependent Tests
- Always pass time as parameter
- Advance time manually in tests
- Never use `millis()` or `delay()` in test code

### 11. Configuration

#### Hardware Pins (Config.h)
```cpp
// Heater
#define HEATER_PWM_PIN 25
#define HEATER_PWM_CHANNEL 0
#define HEATER_PWM_FREQ 1000

// Sensors
#define HEATER_TEMP_PIN 4        // DS18B20
#define BOX_SENSOR_SDA 21        // AM2320
#define BOX_SENSOR_SCL 22

// Buttons
#define BUTTON_SET_PIN 32
#define BUTTON_UP_PIN 33
#define BUTTON_DOWN_PIN 34

// Display (I2C OLED)
#define OLED_SDA 21
#define OLED_SCL 22

// Sound
#define BUZZER_PIN 26
```

#### Constants
```cpp
// Constraints
constexpr uint32_t MAX_TIME_SECONDS = 36000;  // 10 hours
constexpr float MIN_TEMP = 30.0;
constexpr float MAX_BOX_TEMP = 80.0;
constexpr float MAX_HEATER_TEMP = 90.0;

// Update intervals
constexpr uint32_t HEATER_TEMP_INTERVAL = 500;
constexpr uint32_t BOX_DATA_INTERVAL = 2000;
constexpr uint32_t PID_UPDATE_INTERVAL = 500;
constexpr uint32_t STATE_SAVE_INTERVAL = 1000;

// PWM
constexpr uint32_t PWM_PERIOD_MS = 5000;
constexpr uint8_t PWM_RESOLUTION = 8;  // 0-255

// PID Profiles
enum class PIDProfile {
    SOFT,    // Kp=2.0, Ki=0.5, Kd=1.0
    NORMAL,  // Kp=4.0, Ki=1.0, Kd=2.0
    STRONG   // Kp=6.0, Ki=1.5, Kd=3.0
};

// PID Anti-windup and filtering
constexpr float PID_DERIVATIVE_FILTER_ALPHA = 0.2;
constexpr float PID_TEMP_SLOWDOWN_MARGIN = 5.0;  // Start scaling within 5°C
```

### 12. Dependencies

#### Required Libraries
- **mathertel/OneButton**: Button handling
- **ArduinoJson**: Settings serialization
- **LittleFS**: File system
- **Wire**: I2C communication
- **OneWire + DallasTemperature**: DS18B20
- **Adafruit_AM2320**: Temperature/humidity sensor
- **Adafruit_SSD1306** or similar: OLED display

#### Unity PIO Testing
- Unity test framework
- No Arduino libraries in unit tests (mock all hardware)

### 13. Error Handling

#### Sensor Failures
- SensorManager detects invalid readings
- Fires `SensorErrorCallback`
- SafetyMonitor receives error and triggers emergency stop
- System transitions to FAILED state

#### Safety Violations
- SafetyMonitor detects limit breach
- Fires `EmergencyStopCallback`
- Dryer immediately stops heater
- Transitions to FAILED state
- Saves emergency state to storage
- Plays alarm sound

#### Power Loss
- Runtime state saved every second
- On boot, check for recent state file
- If valid (<5 min old), load and transition to POWER_RECOVERED
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
- [ ] Use `std::function<>` for callbacks
- [ ] Provide `register*Callback()` methods
- [ ] Store callbacks in `std::vector<>`
- [ ] Check `if (callback)` before invoking
- [ ] Create production and mock factories
- [ ] Write unit tests with time injection
- [ ] Document responsibilities in header comment
- [ ] Place implementation in appropriate folder (sensors/, control/, userInterface/, storage/)

---

## Example Component Template

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
        // Implementation
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

## PID Controller Detailed Specifications

### Anti-Windup Implementation
```cpp
class PIDController : public IPIDController {
private:
    float integral = 0.0;
    float lastOutput = 0.0;
    
public:
    float compute(float setpoint, float input, uint32_t currentMillis) override {
        // ... calculate dt, error, pTerm ...
        
        // Integral term with anti-windup
        float proposedIntegral = integral + ki * error * dtSec;
        
        // Calculate output with proposed integral
        float proposedOutput = pTerm + proposedIntegral + dTerm;
        
        // Check if output would saturate
        if (proposedOutput > outMax) {
            // Don't let integral grow if we're already maxed out
            if (error > 0) {
                // Error is positive but output is maxed - don't accumulate
                proposedIntegral = integral;
            }
        } else if (proposedOutput < outMin) {
            // Don't let integral shrink if we're already at minimum
            if (error < 0) {
                // Error is negative but output is at min - don't accumulate
                proposedIntegral = integral;
            }
        }
        
        // Apply the integral
        integral = proposedIntegral;
        
        // Clamp integral to reasonable bounds
        integral = constrain(integral, outMin, outMax);
        
        // Calculate final output
        float output = pTerm + integral + dTerm;
        output = constrain(output, outMin, outMax);
        
        lastOutput = output;
        return output;
    }
};
```

### Derivative Smoothing (Low-Pass Filter)
```cpp
class PIDController : public IPIDController {
private:
    float filteredDerivative = 0.0;
    static constexpr float DERIVATIVE_FILTER_ALPHA = 0.2;  // 0-1, lower = more smoothing
    
public:
    float compute(float setpoint, float input, uint32_t currentMillis) override {
        // ... calculate pTerm, integral ...
        
        // Calculate raw derivative (on measurement, not error)
        float dInput = (input - lastInput) / dtSec;
        float rawDerivative = -kd * dInput;  // Negative because we want to oppose change
        
        // Apply exponential moving average (low-pass filter)
        // filtered[n] = α * raw[n] + (1-α) * filtered[n-1]
        filteredDerivative = DERIVATIVE_FILTER_ALPHA * rawDerivative 
                           + (1.0 - DERIVATIVE_FILTER_ALPHA) * filteredDerivative;
        
        float dTerm = filteredDerivative;
        
        // ... rest of calculation ...
    }
    
    void reset() override {
        integral = 0.0;
        filteredDerivative = 0.0;  // Reset filter state too
        lastInput = 0.0;
        firstRun = true;
    }
};
```

### Temperature-Aware Slowdown
```cpp
class PIDController : public IPIDController {
private:
    float maxAllowedTemp = 90.0;
    static constexpr float TEMP_SLOWDOWN_MARGIN = 5.0;  // Start scaling within 5°C
    
public:
    void setMaxAllowedTemp(float maxTemp) {
        maxAllowedTemp = maxTemp;
    }
    
    float compute(float setpoint, float input, uint32_t currentMillis) override {
        // Normal PID calculation
        float output = pTerm + integral + dTerm;
        output = constrain(output, outMin, outMax);
        
        // Temperature-aware slowdown
        float tempMargin = maxAllowedTemp - input;
        
        if (input >= maxAllowedTemp) {
            // Emergency: at or above max temp, stop completely
            output = 0.0;
            // Also stop integral accumulation
            integral = 0.0;
        } else if (tempMargin < TEMP_SLOWDOWN_MARGIN) {
            // Approaching max temp: scale output proportionally
            // At 5°C margin: scale = 1.0 (100%)
            // At 0°C margin: scale = 0.0 (0%)
            float scaleFactor = tempMargin / TEMP_SLOWDOWN_MARGIN;
            output *= scaleFactor;
            
            // Also slow down integral accumulation when near limit
            // This prevents integral windup when we're limiting output
            integral *= scaleFactor;
        }
        
        lastInput = input;
        lastTime = currentMillis;
        
        return output;
    }
};
```

---

## Menu Controller Detailed Specifications

### Menu Structure Definition
```cpp
// Types.h
enum class MenuPath {
    ROOT,
    
    // Status submenu
    STATUS,
    STATUS_START,
    STATUS_PAUSE,
    STATUS_RESET,
    
    // Preset submenu
    PRESET,
    PRESET_PLA,
    PRESET_PETG,
    PRESET_CUSTOM,
    
    // Custom preset submenu
    CUSTOM_TEMP,
    CUSTOM_TIME,
    CUSTOM_OVERSHOOT,
    CUSTOM_SAVE,
    CUSTOM_BACK,
    
    // PID profile submenu
    PID_PROFILE,
    PID_SOFT,
    PID_NORMAL,
    PID_STRONG,
    
    // Sound submenu
    SOUND,
    SOUND_ON,
    SOUND_OFF,
    
    BACK
};

enum class MenuItemType {
    SUBMENU,      // Leads to another menu
    ACTION,       // Executes an action (start, pause, save)
    VALUE_EDIT,   // Editable value (temp, time)
    TOGGLE        // On/off selection
};

struct MenuItem {
    String label;
    MenuItemType type;
    MenuPath path;
    
    // For VALUE_EDIT type
    int currentValue;
    int minValue;
    int maxValue;
    int step;
    String unit;
    
    // For SUBMENU type
    MenuPath submenuPath;
};

using MenuSelectionCallback = std::function<void(MenuPath path, int value)>;
```

### MenuController Implementation Pattern
```cpp
// userInterface/MenuController.h
class MenuController : public IMenuController {
private:
    MenuPath currentMenu;
    int currentSelection;
    bool inEditMode;
    int editValue;
    
    MenuItem editingItem;
    std::vector<MenuSelectionCallback> callbacks;
    
    // Constraints passed from Dryer
    float minTemp;
    float maxTemp;
    uint32_t maxTime;
    float maxOvershoot;
    
    // Current custom preset values (maintained here during editing)
    struct CustomPresetDraft {
        float temp;
        uint32_t time;
        float overshoot;
    } customDraft;
    
public:
    MenuController() : 
        currentMenu(MenuPath::ROOT),
        currentSelection(0),
        inEditMode(false),
        minTemp(30.0),
        maxTemp(80.0),
        maxTime(36000),
        maxOvershoot(10.0) {}
    
    void setConstraints(float min, float max, uint32_t maxT, float maxO) {
        minTemp = min;
        maxTemp = max;
        maxTime = maxT;
        maxOvershoot = maxO;
    }
    
    void setCustomPresetValues(float temp, uint32_t time, float overshoot) {
        customDraft.temp = temp;
        customDraft.time = time;
        customDraft.overshoot = overshoot;
    }
    
    void handleAction(MenuAction action) override {
        if (inEditMode) {
            handleEditMode(action);
        } else {
            handleNavigationMode(action);
        }
    }
    
private:
    void handleNavigationMode(MenuAction action) {
        switch (action) {
            case MenuAction::UP:
                navigateUp();
                break;
                
            case MenuAction::DOWN:
                navigateDown();
                break;
                
            case MenuAction::ENTER:
                selectCurrentItem();
                break;
                
            case MenuAction::BACK:
                navigateBack();
                break;
        }
    }
    
    void handleEditMode(MenuAction action) {
        switch (action) {
            case MenuAction::UP:
                editValue += editingItem.step;
                editValue = constrain(editValue, editingItem.minValue, editingItem.maxValue);
                break;
                
            case MenuAction::DOWN:
                editValue -= editingItem.step;
                editValue = constrain(editValue, editingItem.minValue, editingItem.maxValue);
                break;
                
            case MenuAction::ENTER:
                // Confirm edit
                confirmEdit();
                break;
                
            case MenuAction::BACK:
                // Cancel edit
                cancelEdit();
                break;
        }
    }
    
    void selectCurrentItem() {
        MenuItem item = getCurrentMenuItem();
        
        switch (item.type) {
            case MenuItemType::SUBMENU:
                enterSubmenu(item.submenuPath);
                break;
                
            case MenuItemType::ACTION:
                executeAction(item.path);
                break;
                
            case MenuItemType::VALUE_EDIT:
                enterEditMode(item);
                break;
                
            case MenuItemType::TOGGLE:
                executeAction(item.path);
                break;
        }
    }
    
    void enterEditMode(MenuItem item) {
        inEditMode = true;
        editingItem = item;
        editValue = item.currentValue;
    }
    
    void confirmEdit() {
        inEditMode = false;
        
        // Update draft values
        switch (editingItem.path) {
            case MenuPath::CUSTOM_TEMP:
                customDraft.temp = editValue;
                break;
            case MenuPath::CUSTOM_TIME:
                customDraft.time = editValue;
                break;
            case MenuPath::CUSTOM_OVERSHOOT:
                customDraft.overshoot = editValue;
                break;
        }
        
        // Notify callbacks
        notifyCallbacks(editingItem.path, editValue);
    }
    
    void cancelEdit() {
        inEditMode = false;
        // editValue discarded, draft values unchanged
    }
    
    void executeAction(MenuPath path) {
        // For actions and save, notify with value 0
        notifyCallbacks(path, 0);
        
        // Navigate back after save
        if (path == MenuPath::CUSTOM_SAVE) {
            navigateBack();
        }
    }
    
    MenuItem getCurrentMenuItem() {
        std::vector<MenuItem> items = getCurrentMenuItems();
        return items[currentSelection];
    }
    
    std::vector<MenuItem> getCurrentMenuItems() override {
        switch (currentMenu) {
            case MenuPath::ROOT:
                return getRootMenu();
            case MenuPath::STATUS:
                return getStatusMenu();
            case MenuPath::PRESET:
                return getPresetMenu();
            case MenuPath::PRESET_CUSTOM:
                return getCustomPresetMenu();
            case MenuPath::PID_PROFILE:
                return getPIDProfileMenu();
            case MenuPath::SOUND:
                return getSoundMenu();
            default:
                return getRootMenu();
        }
    }
    
    std::vector<MenuItem> getRootMenu() {
        return {
            {"Status", MenuItemType::SUBMENU, MenuPath::STATUS, 0, 0, 0, 0, "", MenuPath::STATUS},
            {"Preset", MenuItemType::SUBMENU, MenuPath::PRESET, 0, 0, 0, 0, "", MenuPath::PRESET},
            {"PID Profile", MenuItemType::SUBMENU, MenuPath::PID_PROFILE, 0, 0, 0, 0, "", MenuPath::PID_PROFILE},
            {"Sound", MenuItemType::SUBMENU, MenuPath::SOUND, 0, 0, 0, 0, "", MenuPath::SOUND}
        };
    }
    
    std::vector<MenuItem> getCustomPresetMenu() {
        return {
            {"Temp", MenuItemType::VALUE_EDIT, MenuPath::CUSTOM_TEMP, 
             (int)customDraft.temp, (int)minTemp, (int)maxTemp, 1, "°C"},
            {"Time", MenuItemType::VALUE_EDIT, MenuPath::CUSTOM_TIME, 
             (int)(customDraft.time / 60), 0, (int)(maxTime / 60), 10, "min"},
            {"Max Overshoot", MenuItemType::VALUE_EDIT, MenuPath::CUSTOM_OVERSHOOT, 
             (int)customDraft.overshoot, 0, (int)maxOvershoot, 1, "°C"},
            {"Save", MenuItemType::ACTION, MenuPath::CUSTOM_SAVE},
            {"Back", MenuItemType::ACTION, MenuPath::BACK}
        };
    }
    
    void notifyCallbacks(MenuPath path, int value) {
        for (auto& callback : callbacks) {
            if (callback) {
                callback(path, value);
            }
        }
    }
    
public:
    void registerSelectionCallback(MenuSelectionCallback callback) override {
        callbacks.push_back(callback);
    }
    
    bool isInEditMode() const {
        return inEditMode;
    }
    
    MenuItem getEditingItem() const {
        return editingItem;
    }
    
    int getEditValue() const {
        return editValue;
    }
};
```

### UIController Integration with MenuController
```cpp
// userInterface/UIController.h
class UIController {
private:
    IMenuController* menuController;
    IDisplay* display;
    ISoundController* soundController;
    ButtonManager* buttonManager;
    Dryer* dryer;
    
public:
    void setup() {
        // Pass constraints from Dryer to MenuController
        menuController->setConstraints(
            dryer->getMinTemp(),
            dryer->getMaxTemp(),
            dryer->getMaxTime(),
            dryer->getMaxOvershoot()
        );
        
        // Initialize menu with current custom preset values
        CustomPreset preset = dryer->getCustomPreset();
        menuController->setCustomPresetValues(
            preset.targetTemp,
            preset.targetTime,
            preset.maxOvershoot
        );
        
        // Register button callbacks
        setupButtonCallbacks();
        
        // Register menu selection callback
        menuController->registerSelectionCallback(
            [this](MenuPath path, int value) {
                handleMenuSelection(path, value);
            }
        );
        
        // Register dryer stats callback for display updates
        dryer->registerStatsUpdateCallback(
            [this](const CurrentStats& stats) {
                updateDisplay(stats);
            }
        );
    }
    
    void setupButtonCallbacks() {
        buttonManager->registerButtonCallback(ButtonType::SET,
            [this](ButtonEvent event) {
                if (event == ButtonEvent::SINGLE_CLICK) {
                    menuController->handleAction(MenuAction::ENTER);
                    soundController->playClick();
                } else if (event == ButtonEvent::LONG_PRESS) {
                    menuController->handleAction(MenuAction::BACK);
                    soundController->playClick();
                }
            }
        );
        
        buttonManager->registerButtonCallback(ButtonType::UP,
            [this](ButtonEvent event) {
                if (event == ButtonEvent::SINGLE_CLICK) {
                    menuController->handleAction(MenuAction::UP);
                    soundController->playClick();
                }
            }
        );
        
        buttonManager->registerButtonCallback(ButtonType::DOWN,
            [this](ButtonEvent event) {
                if (event == ButtonEvent::SINGLE_CLICK) {
                    menuController->handleAction(MenuAction::DOWN);
                    soundController->playClick();
                }
            }
        );
    }
    
    void handleMenuSelection(MenuPath path, int value) {
        switch (path) {
            case MenuPath::STATUS_START:
                dryer->start();
                soundController->playStart();
                break;
                
            case MenuPath::STATUS_PAUSE:
                dryer->pause();
                break;
                
            case MenuPath::STATUS_RESET:
                dryer->reset();
                break;
                
            case MenuPath::PRESET_PLA:
                dryer->selectPreset(PresetType::PLA);
                soundController->playConfirm();
                break;
                
            case MenuPath::PRESET_PETG:
                dryer->selectPreset(PresetType::PETG);
                soundController->playConfirm();
                break;
                
            case MenuPath::PRESET_CUSTOM:
                dryer->selectPreset(PresetType::CUSTOM);
                soundController->playConfirm();
                break;
                
            case MenuPath::CUSTOM_TEMP:
                dryer->setCustomPresetTemp(value);
                break;
                
            case MenuPath::CUSTOM_TIME:
                dryer->setCustomPresetTime(value * 60);  // Convert minutes to seconds
                break;
                
            case MenuPath::CUSTOM_OVERSHOOT:
                dryer->setCustomPresetOvershoot(value);
                break;
                
            case MenuPath::CUSTOM_SAVE:
                dryer->saveCustomPreset();
                soundController->playConfirm();
                break;
                
            case MenuPath::PID_SOFT:
                dryer->setPIDProfile(PIDProfile::SOFT);
                soundController->playConfirm();
                break;
                
            case MenuPath::PID_NORMAL:
                dryer->setPIDProfile(PIDProfile::NORMAL);
                soundController->playConfirm();
                break;
                
            case MenuPath::PID_STRONG:
                dryer->setPIDProfile(PIDProfile::STRONG);
                soundController->playConfirm();
                break;
                
            case MenuPath::SOUND_ON:
                soundController->setEnabled(true);
                dryer->setSoundEnabled(true);
                soundController->playConfirm();
                break;
                
            case MenuPath::SOUND_OFF:
                dryer->setSoundEnabled(false);
                soundController->setEnabled(false);
                break;
                
            default:
                break;
        }
    }
    
    void updateDisplay(const CurrentStats& stats) {
        if (menuController->isInEditMode()) {
            // Show edit screen
            MenuItem item = menuController->getEditingItem();
            int value = menuController->getEditValue();
            display->showEditScreen(item.label, value, item.unit);
        } else {
            // Show menu or status screen
            std::vector<MenuItem> items = menuController->getCurrentMenuItems();
            display->showMenu(items, stats);
        }
    }
};
```

---

## Complete Data Flow: Custom Preset Editing Example

```
1. User navigates to "Preset" menu
   └─> UIController passes UP/DOWN to MenuController
       └─> MenuController updates currentSelection
           └─> Display shows menu with "PLA", "PETG", "Custom" highlighted

2. User selects "Custom" (presses SET)
   └─> MenuController enters PRESET_CUSTOM submenu
       └─> Display shows: "Temp", "Time", "Max Overshoot", "Save", "Back"

3. User navigates to "Temp" and presses SET
   └─> MenuController.enterEditMode()
       └─> inEditMode = true
       └─> editValue = customDraft.temp (e.g., 50)
       └─> Display shows: "Temp: 50°C" (blinking cursor)

4. User presses UP three times
   └─> Each press: editValue += 1 (step)
       └─> editValue: 50 → 51 → 52 → 53
       └─> Display updates in real-time: "Temp: 53°C"
       └─> Values clamped by minTemp/maxTemp

5. User presses SET to confirm
   └─> MenuController.confirmEdit()
       ├─> customDraft.temp = 53
       ├─> notifyCallbacks(CUSTOM_TEMP, 53)
       │   └─> UIController.handleMenuSelection(CUSTOM_TEMP, 53)
       │       └─> Dryer.setCustomPresetTemp(53)
       │           └─> Dryer updates internal customPreset.targetTemp
       └─> inEditMode = false
       └─> Display returns to menu: "Temp", "Time", ...

6. User edits "Time" and "Max Overshoot" similarly
   └─> Each edit updates customDraft and notifies Dryer

7. User navigates to "Save" and presses SET
   └─> MenuController.executeAction(CUSTOM_SAVE)
       └─> notifyCallbacks(CUSTOM_SAVE, 0)
           └─> UIController.handleMenuSelection(CUSTOM_SAVE, 0)
               └─> Dryer.saveCustomPreset()
                   └─> SettingsStorage.saveCustomPreset(customPreset)
                       └─> Writes to /settings.json:
                           {
                             "customPreset": {
                               "targetTemp": 53,
                               "targetTime": 14400,
                               "maxOvershoot": 8
                             }
                           }
       └─> MenuController navigates back to Preset menu
       └─> SoundController plays confirmation tone

8. Next boot
   └─> SettingsStorage.loadSettings()
       └─> Dryer loads customPreset values
       └─> MenuController initialized with these values
       └─> User sees saved settings when editing again
```
