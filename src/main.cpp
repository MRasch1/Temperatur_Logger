#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4 // GPIO pin connected to the DS18B20 data pin

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  sensors.begin();
  int deviceCount = 0;
  oneWire.reset_search();
  byte address[8];
  while (oneWire.search(address)) {
    deviceCount++;
  }
  Serial.print("Number of devices found on the OneWire bus: ");
  Serial.println(deviceCount);
  if (deviceCount == 0) {
    Serial.println("No devices found. Check wiring.");
  } else {
    Serial.println("DS18B20 Temperature Sensor Initialized");
  }
}

void loop() {
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  if (temperatureC == DEVICE_DISCONNECTED_C) {
    Serial.println("Sensor not found or disconnected.");
  } else {
    Serial.print("Temperature: ");
    Serial.print(temperatureC);
    Serial.println(" °C");
  }
  delay(2000);
}