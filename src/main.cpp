#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiManager.h> // Include WiFiManager library
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <SPIFFS.h>
#include "memory_logger.h"
#include <time.h> // For NTP time synchronization

#define ONE_WIRE_BUS 4      // GPIO pin connected to the DS18B20 data pin
#define RESET_BUTTON_PIN 35 // GPIO pin connected to the reset button (D35)
#define LED_PIN 18          // GPIO pin connected to the LED (D18)

bool serviceMode = false; // Flag to track service mode

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // WebSocket endpoint

String getFormattedTime()
{
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo); // Format: YYYY-MM-DD HH:MM:SS
  return String(buffer);
}

void notifyClients(float temperature)
{
  String message = String(temperature);
  ws.textAll(message); // Send temperature to all connected clients
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    Serial.printf("WebSocket message received: %s\n", (char *)data);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
  }
  else if (type == WS_EVT_DATA)
  {
    handleWebSocketMessage(arg, data, len);
  }
}

void logTemperatureToSPIFFS(float temperature) {
  File file = SPIFFS.open("/temperature_log.txt", FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open log file for writing.");
    return;
  }

  String logEntry = getFormattedTime() + " - " + String(temperature) + " °C\n"; // Ensure newline at the end
  file.print(logEntry);
  file.close();

  // Serial.println("Logged: " + logEntry);
}

// Function to generate a CSV file from log data
bool generateCSV(const char *filePath, const char *header, const String &data)
{
  File csvFile = SPIFFS.open(filePath, FILE_WRITE);
  if (!csvFile)
  {
    Serial.println("Failed to open CSV file for writing.");
    return false;
  }

  csvFile.println(header); // Write the header

  // Add spaces between logs by appending an extra newline after each log entry
  String formattedData = data;
  formattedData.replace("\n", "\n\n"); // Replace single newlines with double newlines

  csvFile.print(formattedData); // Write the formatted log data
  csvFile.close();
  // Serial.println("CSV file generated successfully.");
  return true;
}

void generateCSVFromLogs()
{
  File logFile = SPIFFS.open("/temperature_log.txt", FILE_READ);
  if (!logFile)
  {
    Serial.println("Failed to open log file for generating CSV.");
    return;
  }

  String logs = "";
  while (logFile.available())
  {
    logs += logFile.readStringUntil('\n');
  }
  logFile.close();

  if (generateCSV("/temperature_log.csv", "Timestamp,Temperature", logs))
  {
    // Serial.println("CSV generated successfully.");
  }
  else
  {
    Serial.println("Failed to generate CSV.");
  }
}

void setup()
{
  Serial.begin(115200);

  // Configure the reset button pin
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Configure the LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure the LED is off initially

  // Initialize SPIFFS
  if (!SPIFFS.begin())
  {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }

  // WiFiManager setup
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180); // Timeout for configuration portal (in seconds)
  if (!wifiManager.autoConnect("ESP32-Setup", "password"))
  {
    Serial.println("Failed to connect to Wi-Fi and timed out!");
    ESP.restart(); // Restart if connection fails
  }

  Serial.println("Wi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov"); // Set timezone to Copenhagen (CET/CEST)
  Serial.print("Waiting for NTP time sync...");
  unsigned long startTime = millis();
  while (!time(nullptr))
  {
    Serial.print(".");
    delay(1000);
    if (millis() - startTime > 30000)
    { // Timeout after 30 seconds
      Serial.println("\nFailed to synchronize time.");
      break;
    }
  }
  Serial.println("\nTime synchronized!");

  // Initialize temperature sensor
  sensors.begin();
  int deviceCount = 0;
  oneWire.reset_search();
  byte address[8];
  while (oneWire.search(address))
  {
    deviceCount++;
  }
  Serial.print("Number of devices found on the OneWire bus: ");
  Serial.println(deviceCount);
  if (deviceCount == 0)
  {
    Serial.println("No devices found. Check wiring.");
  }
  else
  {
    Serial.println("DS18B20 Temperature Sensor Initialized");
  }

  // WebSocket setup
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Serve static files from SPIFFS
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // Add endpoints
  server.on("/clear_wifi", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    Serial.println("Wi-Fi settings cleared.");
    request->send(200, "text/plain", "Wi-Fi settings cleared. Restarting...");
    delay(1000);
    serviceMode = false;
    ESP.restart(); });

  // Endpoint to clear temperature data
  server.on("/clear_data", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool csvCleared = false;
    bool txtCleared = false;

    // Clear the CSV file
    if (SPIFFS.exists("/temperature_log.csv")) {
      if (SPIFFS.remove("/temperature_log.csv")) {
        Serial.println("CSV file cleared successfully.");
        csvCleared = true;
      } else {
        Serial.println("Failed to clear CSV file.");
      }
    } else {
      Serial.println("CSV file does not exist.");
    }

    // Clear the TXT log file
    if (SPIFFS.exists("/temperature_log.txt")) {
      if (SPIFFS.remove("/temperature_log.txt")) {
        Serial.println("TXT log file cleared successfully.");
        txtCleared = true;
      } else {
        Serial.println("Failed to clear TXT log file.");
      }
    } else {
      Serial.println("TXT log file does not exist.");
    }

    // Respond to the client
    if (csvCleared || txtCleared) {
      request->send(200, "text/plain", "Temperature data cleared successfully.");
    } else {
      request->send(404, "text/plain", "No temperature data to clear.");
    }
  });

  // Endpoint to check service mode status
  server.on("/is_service_mode", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String jsonResponse = "{\"serviceMode\": " + String(serviceMode ? "true" : "false") + "}";
    request->send(200, "application/json", jsonResponse); });

  // Endpoint to download temperature logs as CSV
  server.on("/download_logs", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (SPIFFS.exists("/temperature_log.csv")) {
      request->send(SPIFFS, "/temperature_log.csv", "text/csv");
    } else {
      request->send(404, "text/plain", "No logs available.");
    } });

  // Endpoint to generate CSV from logs
  server.on("/generate_csv", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    File logFile = SPIFFS.open("/temperature_log.txt", FILE_READ);
    if (!logFile) {
      request->send(500, "text/plain", "Failed to open log file.");
      return;
    }

    String logs = "";
    while (logFile.available()) {
      logs += logFile.readStringUntil('\n');
    }
    logFile.close();

    if (generateCSV("/temperature_log.csv", "Timestamp,Temperature", logs)) {
      request->send(200, "text/plain", "CSV generated successfully.");
    } else {
      request->send(500, "text/plain", "Failed to generate CSV.");
    } });

  // Endpoint to activate service mode
  server.on("/activate_service_mode", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              serviceMode = true;                                          // Activate service mode
              request->send(200, "text/plain", "Service mode activated."); // Respond to the client
            });

  // Endpoint to toggle service mode
  server.on("/toggle_service_mode", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    serviceMode = !serviceMode; // Toggle service mode
    String response = serviceMode ? "Service mode activated." : "Service mode deactivated.";
    request->send(200, "text/plain", response); });

  // Start server
  server.begin();
}

void loop()
{
  static bool buttonPressed = false;          // Flag to track button state
  static unsigned long buttonPressStart = 0;  // Track when the button was first pressed
  static unsigned long elapsedTime = 0;       // Track how long the button has been held
  static unsigned long lastPrintedSecond = 0; // Track the last second that was printed

  // Check if the reset button is pressed
  if (digitalRead(RESET_BUTTON_PIN) == LOW)
  { // Button is pressed
    if (!buttonPressed)
    { // Button was just pressed
      buttonPressed = true;
      buttonPressStart = millis(); // Start timing
      lastPrintedSecond = 0;       // Reset the last printed second
      Serial.println("Button pressed. Starting timer...");
    }

    // Calculate how long the button has been held
    elapsedTime = millis() - buttonPressStart;

    // Print a message for every second the button is held
    unsigned long currentSecond = elapsedTime / 1000;
    if (currentSecond > lastPrintedSecond)
    {
      lastPrintedSecond = currentSecond;
      Serial.print("Button held for: ");
      Serial.print(currentSecond);
      Serial.println(" seconds");
    }

    // Control the LED behavior
    if (elapsedTime / 1000 == 5)
    {
      digitalWrite(LED_PIN, LOW); // Turn off the LED at second 5
    }
    else
    {
      digitalWrite(LED_PIN, HIGH); // Turn on the LED for all other seconds
    }

    return; // Skip the rest of the loop while the button is held
  }
  else
  { // Button is not pressed
    if (buttonPressed)
    {                             // Button was just released
      buttonPressed = false;      // Reset button state
      digitalWrite(LED_PIN, LOW); // Turn off the LED when the button is released

      // Check if the button was released after exactly 5 seconds
      if (elapsedTime >= 5000 && elapsedTime < 6000)
      {
        if (serviceMode)
        {
          Serial.println("Button released after 5 seconds. Deactivating service mode...");
          serviceMode = false; // Deactivate service mode
        }
        else
        {
          Serial.println("Button released after 5 seconds. Activating service mode...");
          serviceMode = true; // Activate service mode
        }
      }
      else if (elapsedTime >= 10000)
      { // If the button was held for 10 seconds
        Serial.println("Button released after 10 seconds. Resetting Wi-Fi settings...");
        WiFiManager wifiManager;
        wifiManager.resetSettings(); // Clear saved Wi-Fi credentials
        delay(1000);                 // Small delay to ensure resetSettings completes
        Serial.println("Restarting ESP...");
        serviceMode = false; // Exit service mode
        ESP.restart();       // Restart the ESP32
      }

      Serial.println("Button released. Timer reset.");
    }
  }

  // // If not in service mode, continue normal operation
  // if (!serviceMode) {
  // Request temperature readings from the sensor
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);

  // Check if the sensor is disconnected
  if (temperatureC == DEVICE_DISCONNECTED_C)
  {
    Serial.println("Sensor not found or disconnected.");
  }
  else
  {
    Serial.print("Temperature: ");
    Serial.print(temperatureC);
    Serial.println(" °C");

    // Log temperature to SPIFFS
    // logTemperatureToSPIFFS(temperatureC);

    // Send temperature updates to WebSocket clients every 5 minutes
    static unsigned long lastTemperatureUpdate = 0;
    if (millis() - lastTemperatureUpdate > 5000)
    {
      notifyClients(temperatureC);
      logTemperatureToSPIFFS(temperatureC); // Log temperature to SPIFFS
      lastTemperatureUpdate = millis();
      // Generate CSV from logs
      generateCSVFromLogs();
    }
  }

  // Delay for 2 seconds before the next reading
  delay(2000);
}