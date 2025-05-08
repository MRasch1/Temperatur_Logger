#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiManager.h> // Include WiFiManager library
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <SPIFFS.h>

#define ONE_WIRE_BUS 4    // GPIO pin connected to the DS18B20 data pin
#define RESET_BUTTON_PIN 35 // GPIO pin connected to the reset button (D35)
#define LED_PIN 18         // GPIO pin connected to the LED (D18)

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // WebSocket endpoint

void notifyClients(float temperature) {
  String message = String(temperature);
  ws.textAll(message); // Send temperature to all connected clients
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    Serial.printf("WebSocket message received: %s\n", (char *)data);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

void setup() {
  Serial.begin(115200);

  // Configure the reset button pin
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Configure the LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure the LED is off initially

  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }

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

  // WebSocket setup
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Serve static files from SPIFFS
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // Start server
  server.begin();
}

void loop() {
  static bool buttonPressed = false; // Flag to track button state
  static unsigned long buttonPressStart = 0; // Track when the button was first pressed
  static unsigned long lastPrintedTime = 0; // Track the last time a message was printed
  static unsigned long lastTemperatureUpdate = 0; // Track the last time temperature was sent

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

    // Send temperature updates to WebSocket clients every 2 seconds
    if (millis() - lastTemperatureUpdate > 2000) {
      notifyClients(temperatureC);
      lastTemperatureUpdate = millis();
    }
  }

  // Delay for 2 seconds before the next reading
  delay(2000);
}