# BME280 Sensor Provider

A [Sensor Hub](../sensor-hub/readme.md) provider usermod for the Bosch
BME280 - registers `bme280_temperature`, `bme280_humidity` and
`bme280_pressure` with the hub by default, which then handles MQTT, Home
Assistant discovery, the JSON API and the Info tab.

## Hardware

Wire SDA/SCL to the I2C pins configured on WLED's own **Config > LED
Preferences** page (shared across all I2C usermods). This usermod does not
call `Wire.begin()` itself. Both common breakout-board addresses (`0x76`
and `0x77`) are probed automatically. Retries `begin()` every 10s if the
sensor isn't found; after 3 consecutive failed reads all three sensors are
marked unavailable in Home Assistant, after 10 it re-attempts `begin()`.

## Usage

Self-contained out-of-tree usermod (see `library.json` for its
`adafruit/Adafruit BME280 Library` dependency). Add it to
`custom_usermods` next to the [Sensor Hub](../sensor-hub/readme.md) itself,
same as the [SHTC3 provider](../sensor-hub-shtc3-provider/readme.md).

## Usermod Settings

| Setting | Default | Description |
|---|---|---|
| Enabled | on | Master on/off switch (also auto-disabled if I2C pins aren't configured) |
| Check interval | 30s | How often the sensor is read |
| Name prefix | `bme280` | Sensor names become `<prefix>_temperature/_humidity/_pressure` - must be unique across every provider registered with the hub |
| Precision | 1 | Decimal places published for all three readings |
