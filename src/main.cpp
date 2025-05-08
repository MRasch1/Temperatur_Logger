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

bool serviceMode = false; // Flag to track service mode

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

void logTemperatureToSPIFFS(float temperature) {
  File file = SPIFFS.open("/temperature_log.txt", FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open log file for writing.");
    return;
  }

  String logEntry = String(millis() / 1000) + "s: " + String(temperature) + " °C\n";
  file.print(logEntry);
  file.close();
  Serial.println("Logged: " + logEntry);
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

  // Add endpoints
  server.on("/clear_wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    Serial.println("Wi-Fi settings cleared.");
    request->send(200, "text/plain", "Wi-Fi settings cleared. Restarting...");
    delay(1000);
    serviceMode = false;
    ESP.restart();
  });

// Endpoint to clear temperature data
  server.on("/clear_data", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/temperature_log.txt")) {
      SPIFFS.remove("/temperature_log.txt");
      Serial.println("Temperature data cleared.");
      request->send(200, "text/plain", "Temperature data cleared.");
    } else {
      request->send(404, "text/plain", "No temperature data to clear.");
    }
  });

// Endpoint to check service mode status
  server.on("/is_service_mode", HTTP_GET, [](AsyncWebServerRequest *request) {
    String jsonResponse = "{\"serviceMode\": " + String(serviceMode ? "true" : "false") + "}";
    request->send(200, "application/json", jsonResponse);
  });

  // Start server
  server.begin();
}

void loop() {
  static bool buttonPressed = false; // Flag to track button state
  static unsigned long buttonPressStart = 0; // Track when the button was first pressed
  static unsigned long lastPrintedTime = 0; // Track the last time a message was printed
  static bool serviceModeActivated = false; // Flag to ensure service mode is activated only once
  static bool serviceModeDeactivated = false; // Flag to ensure service mode is not reactivated after deactivation

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

    // Control the LED behavior
    if (elapsedTime / 1000 == 5) {
      digitalWrite(LED_PIN, LOW); // Turn off the LED at second 5
    } else {
      digitalWrite(LED_PIN, HIGH); // Turn on the LED for all other seconds
    }

    // If the button is held for 5 seconds and service mode is already activated, deactivate it
    if (elapsedTime >= 5000 && elapsedTime < 6000 && serviceMode) {
      Serial.println("Button held for 5 seconds. Deactivating service mode...");
      serviceMode = false; // Deactivate service mode
      serviceModeActivated = false; // Reset the activation flag
      serviceModeDeactivated = true; // Mark service mode as deactivated
    }

    // If the button is held for 10 seconds, reset Wi-Fi settings
    if (elapsedTime >= 10000) { // 10 seconds
      Serial.println("Button held for 10 seconds. Resetting Wi-Fi settings...");
      WiFiManager wifiManager;
      wifiManager.resetSettings(); // Clear saved Wi-Fi credentials
      delay(1000); // Small delay to ensure resetSettings completes
      Serial.println("Restarting ESP...");
      serviceMode = false; // Exit service mode
      ESP.restart(); // Restart the ESP32
    }

    return; // Skip the rest of the loop while the button is held
  } else { // Button is not pressed
    if (buttonPressed) { // Button was just released
      unsigned long elapsedTime = millis() - buttonPressStart;

      // Check if the button was released at exactly 5 seconds
      if (elapsedTime >= 5000 && elapsedTime < 6000 && !serviceModeActivated && !serviceModeDeactivated) {
        Serial.println("Button released at 5 seconds. Activating service mode...");
        serviceMode = true; // Mark service mode as activated
        serviceModeActivated = true; // Ensure this message is printed only once
      }

      buttonPressed = false; // Reset button state
      Serial.println("Button released. Timer reset.");
    }
    digitalWrite(LED_PIN, LOW); // Turn off the LED when the button is released
  }

  // If not in service mode, continue normal operation
  if (!serviceMode) {
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

      // Log temperature to SPIFFS
      logTemperatureToSPIFFS(temperatureC);

      // Send temperature updates to WebSocket clients every 2 seconds
      static unsigned long lastTemperatureUpdate = 0;
      if (millis() - lastTemperatureUpdate > 2000) {
        notifyClients(temperatureC);
        logTemperatureToSPIFFS(temperatureC); // Log temperature to SPIFFS
        lastTemperatureUpdate = millis();
      }
    }

    // Delay for 2 seconds before the next reading
    delay(2000);
  }
}