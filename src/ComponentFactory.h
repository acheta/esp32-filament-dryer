#ifndef COMPONENT_FACTORY_H
#define COMPONENT_FACTORY_H

#include "interfaces/ISensorManager.h"
#include "interfaces/IHeaterTempSensor.h"
#include "interfaces/IBoxTempHumiditySensor.h"
#include "interfaces/IDryer.h"
#include "interfaces/IDisplay.h"
#include "interfaces/IFanControl.h"
#include "sensors/SensorManager.h"
#include "sensors/HeaterTempSensor.h"
#include "sensors/BoxTempHumiditySensor.h"
#include "userInterface/OLEDDisplay.h"
#include "control/FanControl.h"
#include "Dryer.h"
#include "Config.h"

#ifdef UNIT_TEST
#include "test/mocks/MockSensorManager.h"
#include "test/mocks/MockHeaterTempSensor.h"
#include "test/mocks/MockBoxTempHumiditySensor.h"
#include "test/mocks/MockDryer.h"
#include "test/mocks/MockDisplay.h"
#include "test/mocks/MockFanControl.h"
#endif

/**
 * Factory interfaces for creating production or mock components
 */

// ==================== Heater Temp Sensor Factory ====================

class IHeaterTempSensorFactory {
public:
    virtual IHeaterTempSensor* create() = 0;
    virtual ~IHeaterTempSensorFactory() = default;
};

class ProductionHeaterTempSensorFactory : public IHeaterTempSensorFactory {
public:
    IHeaterTempSensor* create() override {
        return new HeaterTempSensor(HEATER_TEMP_PIN);
    }
};

#ifdef UNIT_TEST
class MockHeaterTempSensorFactory : public IHeaterTempSensorFactory {
public:
    IHeaterTempSensor* create() override {
        return new MockHeaterTempSensor();
    }
};
#endif

// ==================== Box Temp/Humidity Sensor Factory ====================

class IBoxTempHumiditySensorFactory {
public:
    virtual IBoxTempHumiditySensor* create() = 0;
    virtual ~IBoxTempHumiditySensorFactory() = default;
};

class ProductionBoxTempHumiditySensorFactory : public IBoxTempHumiditySensorFactory {
public:
    IBoxTempHumiditySensor* create() override {
        return new BoxTempHumiditySensor();
    }
};

#ifdef UNIT_TEST
class MockBoxTempHumiditySensorFactory : public IBoxTempHumiditySensorFactory {
public:
    IBoxTempHumiditySensor* create() override {
        return new MockBoxTempHumiditySensor();
    }
};
#endif

// ==================== SensorManager Factory ====================

class ISensorManagerFactory {
public:
    virtual ISensorManager* create() = 0;
    virtual ~ISensorManagerFactory() = default;
};

class ProductionSensorManagerFactory : public ISensorManagerFactory {
private:
    IHeaterTempSensor* heaterSensor;
    IBoxTempHumiditySensor* boxSensor;

public:
    ProductionSensorManagerFactory() {
        // Create sensor instances
        heaterSensor = new HeaterTempSensor(HEATER_TEMP_PIN);
        boxSensor = new BoxTempHumiditySensor();
    }

    ~ProductionSensorManagerFactory() {
        // Note: SensorManager will NOT delete these, so factory manages lifecycle
        // In production code, consider smart pointers or different ownership model
    }

    ISensorManager* create() override {
        return new SensorManager(heaterSensor, boxSensor);
    }
};

#ifdef UNIT_TEST
class MockSensorManagerFactory : public ISensorManagerFactory {
public:
    ISensorManager* create() override {
        // For tests, use MockSensorManager which doesn't need sensor objects
        return new MockSensorManager();
    }
};
#endif

// ==================== Display Factory ====================

class IDisplayFactory {
public:
    virtual IDisplay* create() = 0;
    virtual ~IDisplayFactory() = default;
};

class ProductionDisplayFactory : public IDisplayFactory {
public:
    IDisplay* create() override {
        return new OLEDDisplay(128, 32, 0x3C);
    }
};

#ifdef UNIT_TEST
class MockDisplayFactory : public IDisplayFactory {
public:
    IDisplay* create() override {
        return new MockDisplay();
    }
};
#endif

// ==================== Fan Control Factory ====================

class IFanControlFactory {
public:
    virtual IFanControl* create() = 0;
    virtual ~IFanControlFactory() = default;
};

class ProductionFanControlFactory : public IFanControlFactory {
public:
    IFanControl* create() override {
        return new FanControl(FAN_PIN);
    }
};

#ifdef UNIT_TEST
class MockFanControlFactory : public IFanControlFactory {
public:
    IFanControl* create() override {
        return new MockFanControl();
    }
};
#endif

// ==================== Dryer Factory ====================

class IDryerFactory {
public:
    virtual IDryer* create(
        ISensorManager* sensors,
        IHeaterControl* heater,
        IPIDController* pid,
        ISafetyMonitor* safety,
        ISettingsStorage* storage,
        ISoundController* sound = nullptr,
        IFanControl* fan = nullptr
    ) = 0;
    virtual ~IDryerFactory() = default;
};

class ProductionDryerFactory : public IDryerFactory {
public:
    IDryer* create(
        ISensorManager* sensors,
        IHeaterControl* heater,
        IPIDController* pid,
        ISafetyMonitor* safety,
        ISettingsStorage* storage,
        ISoundController* sound = nullptr,
        IFanControl* fan = nullptr
    ) override {
        return new Dryer(sensors, heater, pid, safety, storage, sound, fan);
    }
};

#ifdef UNIT_TEST
class MockDryerFactory : public IDryerFactory {
public:
    IDryer* create(
        ISensorManager* sensors,
        IHeaterControl* heater,
        IPIDController* pid,
        ISafetyMonitor* safety,
        ISettingsStorage* storage,
        ISoundController* sound = nullptr,
        IFanControl* fan = nullptr
    ) override {
        // Ignore all dependencies and return mock
        (void)sensors;
        (void)heater;
        (void)pid;
        (void)safety;
        (void)storage;
        (void)sound;
        (void)fan;
        return new MockDryer();
    }
};
#endif

// ==================== Factory Helper Functions ====================

/**
 * Create appropriate factories based on build configuration
 */
class ComponentFactoryProvider {
public:
    static IHeaterTempSensorFactory* getHeaterTempSensorFactory() {
#ifdef UNIT_TEST
        return new MockHeaterTempSensorFactory();
#else
        return new ProductionHeaterTempSensorFactory();
#endif
    }

    static IBoxTempHumiditySensorFactory* getBoxTempHumiditySensorFactory() {
#ifdef UNIT_TEST
        return new MockBoxTempHumiditySensorFactory();
#else
        return new ProductionBoxTempHumiditySensorFactory();
#endif
    }

    static ISensorManagerFactory* getSensorManagerFactory() {
#ifdef UNIT_TEST
        return new MockSensorManagerFactory();
#else
        return new ProductionSensorManagerFactory();
#endif
    }

    static IDryerFactory* getDryerFactory() {
#ifdef UNIT_TEST
        return new MockDryerFactory();
#else
        return new ProductionDryerFactory();
#endif
    }

    static IDisplayFactory* getDisplayFactory() {
#ifdef UNIT_TEST
        return new MockDisplayFactory();
#else
        return new ProductionDisplayFactory();
#endif
    }

    static IFanControlFactory* getFanControlFactory() {
#ifdef UNIT_TEST
        return new MockFanControlFactory();
#else
        return new ProductionFanControlFactory();
#endif
    }
};

#endif