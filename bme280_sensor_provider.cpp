#include "wled.h"
#include "sensor_bus.h"
#include <Adafruit_BME280.h>

/*
 * BME280 temperature + humidity + pressure sensor provider.
 *
 * Reads a Bosch BME280 over I2C (address 0x76 or 0x77 - both are probed)
 * and pushes the three readings into the Sensor Hub (see
 * ../sensor-hub/usermod_sensor_hub.cpp and ../sensor-hub/sensor_bus.h) as
 * "<prefix>_temperature", "<prefix>_humidity" and "<prefix>_pressure".
 * This usermod never talks to MQTT, the JSON API or the Info tab itself -
 * the hub takes care of all of that once a sensor is registered here.
 *
 * Wiring: SDA/SCL go to the I2C pins configured on WLED's own Config > LED
 * Preferences page (the shared "i2c_sda"/"i2c_scl" globals). WLED core
 * already calls Wire.begin() with those pins while loading cfg.json at
 * boot (wled00/cfg.cpp), before any usermod's setup() runs - so this
 * usermod only needs to confirm the pins are set, then use the shared Wire
 * bus. It must NOT call Wire.begin() itself.
 */
class BME280SensorUsermod : public Usermod {
  private:
    Adafruit_BME280 bme;
    SensorHub* hub = nullptr;
    uint8_t tempHandle = SENSOR_HANDLE_INVALID;
    uint8_t humidityHandle = SENSOR_HANDLE_INVALID;
    uint8_t pressureHandle = SENSOR_HANDLE_INVALID;

    bool enabled = true;
    bool sensorFound = false;
    bool initDone = false;

    unsigned long lastRead = 0;
    unsigned long lastBeginAttempt = 0;
    uint8_t consecutiveFailures = 0;

    // config
    uint16_t checkIntervalS = 30; // how often to read the sensor
    String namePrefix = "bme280"; // sensor names become "<prefix>_temperature/_humidity/_pressure"
    uint8_t precision = 1;        // decimal places published for all three readings

    static const char _name[];
    static const char _enabled[];
    static const char _checkInterval[];
    static const char _namePrefix[];
    static const char _precision[];

    bool beginSensor() {
      // BME280 breakout boards commonly strap the address to either 0x76 or 0x77.
      return bme.begin(0x76, &Wire) || bme.begin(0x77, &Wire);
    }

    void registerSensors() {
      if (!hub || tempHandle != SENSOR_HANDLE_INVALID) return; // already registered
      tempHandle     = hub->registerSensor((namePrefix + "_temperature").c_str(), SensorType::Temperature, nullptr, nullptr, precision);
      humidityHandle = hub->registerSensor((namePrefix + "_humidity").c_str(),    SensorType::Humidity,    nullptr, nullptr, precision);
      pressureHandle = hub->registerSensor((namePrefix + "_pressure").c_str(),    SensorType::Pressure,    nullptr, nullptr, precision);
    }

    void setSensorsAvailable(bool available) {
      if (!hub) return;
      if (tempHandle != SENSOR_HANDLE_INVALID)     hub->setSensorAvailable(tempHandle, available);
      if (humidityHandle != SENSOR_HANDLE_INVALID) hub->setSensorAvailable(humidityHandle, available);
      if (pressureHandle != SENSOR_HANDLE_INVALID) hub->setSensorAvailable(pressureHandle, available);
    }

  public:
    void setup() override {
      // I2C bus is configured (and Wire.begin() already called) via WLED's
      // own Config > LED Preferences page - nothing to do here if it's unset.
      if (i2c_sda < 0 || i2c_scl < 0) { enabled = false; return; }
      sensorFound = beginSensor();
      initDone = true;
    }

    void loop() override {
      if (!enabled || !initDone) return;

      if (!hub) hub = getSensorHub(); // Sensor Hub usermod may finish init after us
      if (hub) registerSensors();

      unsigned long now = millis();

      if (!sensorFound) {
        // sensor missing at boot (or lost) - keep retrying rather than giving up forever
        if (now - lastBeginAttempt < 10000) return;
        lastBeginAttempt = now;
        sensorFound = beginSensor();
        if (!sensorFound) return;
      }

      if (now - lastRead < (unsigned long)checkIntervalS * 1000UL) return;
      lastRead = now;

      float t = bme.readTemperature();
      float h = bme.readHumidity();
      float p = bme.readPressure() / 100.0f; // Pa -> hPa

      if (isnan(t) || isnan(h) || isnan(p)) {
        consecutiveFailures++;
        if (consecutiveFailures >= 3) setSensorsAvailable(false);
        if (consecutiveFailures >= 10) sensorFound = false; // force a fresh begin() next loop
        return;
      }

      consecutiveFailures = 0;
      setSensorsAvailable(true);
      if (hub) {
        if (tempHandle != SENSOR_HANDLE_INVALID)     hub->updateSensor(tempHandle, t);
        if (humidityHandle != SENSOR_HANDLE_INVALID) hub->updateSensor(humidityHandle, h);
        if (pressureHandle != SENSOR_HANDLE_INVALID) hub->updateSensor(pressureHandle, p);
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_checkInterval)] = checkIntervalS;
      top[FPSTR(_namePrefix)] = namePrefix;
      top[FPSTR(_precision)] = precision;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top[FPSTR(_checkInterval)], checkIntervalS);
      configComplete &= getJsonValue(top[FPSTR(_namePrefix)], namePrefix);
      configComplete &= getJsonValue(top[FPSTR(_precision)], precision);
      return configComplete;
    }

    void appendConfigData(Print& settingsScript) override {
      settingsScript.print(F("addInfo('BME280Sensor:checkInterval',1,'seconds between sensor reads');"));
      settingsScript.print(F("addInfo('BME280Sensor:namePrefix',1,'sensor names become &lt;prefix&gt;_temperature/_humidity/_pressure - must be unique across all sensor providers');"));
      settingsScript.print(F("addInfo('BME280Sensor:precision',1,'decimal places published for all three readings');"));
    }
};

const char BME280SensorUsermod::_name[]          PROGMEM = "BME280Sensor";
const char BME280SensorUsermod::_enabled[]       PROGMEM = "enabled";
const char BME280SensorUsermod::_checkInterval[] PROGMEM = "checkInterval";
const char BME280SensorUsermod::_namePrefix[]    PROGMEM = "namePrefix";
const char BME280SensorUsermod::_precision[]     PROGMEM = "precision";

static BME280SensorUsermod bme280_sensor;
REGISTER_USERMOD(bme280_sensor);
