#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiManager.h> // Include WiFiManager library

#define ONE_WIRE_BUS 4    // GPIO pin connected to the DS18B20 data pin
#define RESET_BUTTON_PIN 35 // GPIO pin connected to the reset button (D35)
#define LED_PIN 18         // GPIO pin connected to the LED (D18)

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);

  // Configure the reset button pin
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Configure the LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure the LED is off initially

  // WiFiManager setup
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180); // Timeout for configuration portal (in seconds)
  if (!wifiManager.autoConnect("ESP32-Setup", "password")) {
    Serial.println("Failed to connect to Wi-Fi and timed out!");
    ESP.restart(); // Restart if connection fails
  }

  Serial.println("Wi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Initialize temperature sensor
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
  static bool buttonPressed = false; // Flag to track button state
  static unsigned long buttonPressStart = 0; // Track when the button was first pressed
  static unsigned long lastPrintedTime = 0; // Track the last time a message was printed

  // Check if the reset button is pressed
  if (digitalRead(RESET_BUTTON_PIN) == LOW) { // Button is pressed
    if (!buttonPressed) { // Button was just pressed
      buttonPressed = true;
      buttonPressStart = millis(); // Start timing
      lastPrintedTime = 0; // Reset the last printed time
      Serial.println("Button pressed. Starting timer...");
    }

    // Calculate how long the button has been held
    unsigned long elapsedTime = millis() - buttonPressStart;

    // Print the elapsed time only once per second
    if (elapsedTime / 1000 > lastPrintedTime) {
      lastPrintedTime = elapsedTime / 1000;
      Serial.print("Button held for: ");
      Serial.print(lastPrintedTime);
      Serial.println(" seconds");
    }

    // If the button is held for more than 10 seconds, reset Wi-Fi settings
    if (elapsedTime > 10000) { // 10 seconds
      Serial.println("Reset button held for 10 seconds. Resetting Wi-Fi settings...");
      WiFiManager wifiManager;
      wifiManager.resetSettings(); // Clear saved Wi-Fi credentials
      delay(1000); // Small delay to ensure resetSettings completes
      Serial.println("Restarting ESP...");
      ESP.restart(); // Restart the ESP32
    }

    digitalWrite(LED_PIN, HIGH); // Turn on the LED while the button is held down
    return; // Skip the rest of the loop
  } else { // Button is not pressed
    if (buttonPressed) { // Button was just released
      buttonPressed = false;
      Serial.println("Button released. Timer reset.");
    }
    digitalWrite(LED_PIN, LOW); // Turn off the LED when the button is released
  }

  // Request temperature readings from the sensor
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);

  // Check if the sensor is disconnected
  if (temperatureC == DEVICE_DISCONNECTED_C) {
    Serial.println("Sensor not found or disconnected.");
  } else {
    Serial.print("Temperature: ");
    Serial.print(temperatureC);
    Serial.println(" °C");
  }

  // Delay for 2 seconds before the next reading
  delay(2000);
}