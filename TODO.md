# TODO List - ESP32 Filament Dryer Project

## Missing Component Implementations

### HIGH PRIORITY

#### 1. SoundController Implementation
- **Status**: Interface exists (`ISoundController.h`), but no implementation
- **Location**: Should be in `src/userInterface/SoundController.h`
- **Dependencies**: Buzzer pin defined in Config.h
- **Methods to implement**:
  - `begin()` - Initialize buzzer pin
  - `playClick()` - Short beep for button feedback
  - `playConfirm()` - Confirmation tone
  - `playStart()` - Start cycle sound
  - `playFinished()` - Completion melody
  - `playAlarm()` - Emergency/failure alarm
  - `setEnabled(bool)` - Enable/disable sounds
- **Mock**: Already exists in `test/mocks/MockSoundController.h`
- **Priority**: HIGH - Referenced throughout codebase but not implemented

### MEDIUM PRIORITY

#### 2. UIController Tests ✅ COMPLETED
- **Status**: ✅ Implemented and passing
- **Location**: `test/test_ui_controller/test_ui_controller.cpp`
- **Tests**: 49 tests covering:
  - ✅ Initialization and component registration
  - ✅ Button callback handling (SET/UP/DOWN in HOME and MENU modes)
  - ✅ Menu mode switching and navigation
  - ✅ Stats screen cycling (6 screens)
  - ✅ Display update dirty flag logic
  - ✅ Menu timeout behavior (30 seconds)
  - ✅ Cached value change detection
  - ✅ Menu selection routing to Dryer methods
  - ✅ Sound integration
  - ✅ Complete integration flows

#### 3. MenuController Tests ✅ COMPLETED
- **Status**: ✅ Implemented and passing
- **Location**: `test/test_menu_controller/test_menu_controller.cpp`
- **Tests**: 37 tests covering:
  - ✅ Menu navigation (UP/DOWN/ENTER/BACK)
  - ✅ Edit mode entry and exit
  - ✅ Value editing with constraints
  - ✅ Menu history navigation
  - ✅ Timer adjustment logic
  - ✅ Custom preset editing
  - ✅ All menu paths accessible
  - ✅ Callback system
  - ✅ Edge cases and integration flows

### LOW PRIORITY

#### 5. Integration Tests
- **Status**: Partial - has `test_dryer_integration.cpp` and `test_sensor_integration.cpp`
- **Additional tests needed**:
  - UI → Dryer integration
  - Menu → Dryer state changes
  - Power recovery flow end-to-end
  - Emergency stop full flow

## Documentation Discrepancies

### Specification.MD Updates Needed

#### 1. UIController Stats Screens
- **Issue**: Specification only mentions 3 stats screens (BOX_TEMP, REMAINING, HEATER_TEMP)
- **Reality**: Implementation has 6 screens including STATUS_OVERVIEW, PRESET_CONFIG, and SENSOR_READINGS
- **Line**: specification.MD:125-129
- **Action**: Update specification to document all 6 stats screens

#### 2. Menu Tree - Missing "Select Preset" Submenu
- **Issue**: specification.MD:616 shows "Select Preset" but doesn't show it actually selects the preset
- **Reality**: In MenuController, selecting from preset menu immediately applies preset and exits
- **Action**: Clarify that preset selection is immediate, not requiring confirmation

#### 3. Menu Tree - "Edit Custom" vs "Custom" Preset
- **Issue**: Specification shows preset menu with "Custom" option, but also "Edit Custom preset" as separate root item
- **Reality**: "Select Preset" menu has "Custom" which SELECTS the custom preset. "Edit Custom" edits parameters
- **Action**: Clarify distinction between selecting custom preset vs editing its values

#### 4. ABS Preset Missing from Specification
- **Issue**: Config.h defines ABS preset but specification.MD only mentions PLA, PETG, CUSTOM
- **Reality**: ABS preset exists in Config.h but not in menu implementation
- **Action**: Either remove ABS from Config.h or add it to menu and docs

### CLAUDE.md Updates Needed

#### 1. Missing Test Commands
- **Issue**: CLAUDE.md shows test commands but doesn't list all test suites
- **Reality**: Missing test_ui_controller, test_menu_controller, test_button_manager
- **Action**: Note these are TODO items in the test command list

## Feature Gaps

### 1. ABS Preset Support
- **Status**: Defined in Config.h but not accessible via menu
- **Required changes**:
  - Add ABS option to MenuController preset menu
  - Add MenuPath::PRESET_ABS to Types.h
  - Add handler in UIController::handleMenuSelection
  - Update PresetType enum if needed
- **Decision needed**: Keep ABS or remove from Config.h?

### 2. System Info Screen - Back Button
- **Issue**: Back button appears in system info list but with special handling
- **Current behavior**: Works but could be cleaner
- **Improvement**: Consider separate rendering for navigation vs info items

### 3. Power Recovery Timeout Check
- **Status**: POWER_RECOVERY_TIMEOUT defined in Config.h but not used
- **Current behavior**: Any saved runtime state triggers recovery
- **Improvement**: Check timestamp against POWER_RECOVERY_TIMEOUT to ignore stale states

### 4. Sensor Async DS18B20 Pattern
- **Status**: Specified in spec (lines 43-44) with async pattern
- **Implementation status**: Need to verify if HeaterTempSensor uses async pattern
- **Pattern**: `requestConversion()` → wait → `isConversionReady()` → `read()`

## Code Quality Improvements

### 1. Error Logging
- **Issue**: Serial.println scattered throughout for debugging
- **Improvement**: Centralized logging with levels (DEBUG, INFO, WARN, ERROR)
- **Benefit**: Can disable in production, easier to filter

### 2. Constexpr Usage
- **Status**: Good use of constexpr in Config.h
- **Improvement**: Ensure all PID constants use constexpr where possible

### 3. Mock Consistency
- **Status**: Most components have mocks
- **Gap**: No mock for SoundController implementation (only interface mock exists)

## Testing Coverage Gaps

### Current Test Coverage
- ✅ test_dryer_integration
- ✅ test_pid_controller
- ✅ test_safety_monitor
- ✅ test_sensor_integration
- ✅ test_display
- ✅ test_heater_control
- ✅ test_fan_control
- ✅ test_settings_storage
- ✅ test_button_manager (18 tests)
- ✅ test_menu_controller (37 tests)
- ✅ test_ui_controller (49 tests)

### Missing Test Coverage
- ❌ test_sound_controller (once implemented)
- ❌ test_box_temp_humidity_sensor
- ❌ test_heater_temp_sensor
- ❌ test_sensor_manager (async patterns)

## Completed Items (For Reference)

### ✅ Completed Features
- Core Dryer state machine with all 6 states
- PID controller with anti-windup and predictive cooling
- HeaterControl with software PWM
- SafetyMonitor with emergency stop
- FanControl with proper pause behavior
- SettingsStorage with LittleFS + JSON
- SensorManager architecture (implementation needs verification)
- MenuController with timer adjustment and custom preset editing
- UIController with dirty flag optimization and 6 stats screens
- Power recovery mechanism
- Preset change during operation with timer reset
- Comprehensive mocking infrastructure

## Notes

- All interface definitions are complete and well-structured
- Dependency injection pattern consistently applied
- Time injection for testability is properly implemented
- Single-file header pattern (no .cpp) maintained throughout
- Mock infrastructure is comprehensive and well-designed
- The architecture is solid - main gaps are in implementation completeness and test coverage

---

**Last Updated**: 2025-10-15
**Review Frequency**: Weekly
**Owner**: Development Team
