#include <OneWire.h>               // Library for OneWire communication (used by DS18B20 temperature sensor)
#include <DallasTemperature.h>     // Library for DS18B20 temperature sensor
#include <WiFiManager.h>           // Library for managing Wi-Fi connections
#include <ESPAsyncWebServer.h>     // Library for asynchronous web server
#include <AsyncTCP.h>              // Library for asynchronous TCP communication
#include <FS.h>                    // File system library
#include <SPIFFS.h>                // SPI Flash File System library
#include "memory_logger.h"         // Custom memory logger (assumed to be user-defined)
#include <time.h>                  // Library for time synchronization using NTP

// Pin definitions
#define ONE_WIRE_BUS 4             // GPIO pin connected to the DS18B20 data pin
#define RESET_BUTTON_PIN 35        // GPIO pin connected to the reset button
#define LED_PIN 18                 // GPIO pin connected to the LED

// Global variables
bool serviceMode = false;          // Flag to track whether the device is in service mode

// OneWire and temperature sensor setup
OneWire oneWire(ONE_WIRE_BUS);     // Initialize OneWire communication
DallasTemperature sensors(&oneWire); // Initialize DallasTemperature library with OneWire instance

// Web server and WebSocket setup
AsyncWebServer server(80);         // Create an asynchronous web server on port 80
AsyncWebSocket ws("/ws");          // WebSocket endpoint for real-time communication with clients

// Function to get the current time as a formatted string
String getFormattedTime()
{
  time_t now = time(nullptr);      // Get the current time
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);    // Convert time to local time structure

  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo); // Format: YYYY-MM-DD HH:MM:SS
  return String(buffer);           // Return the formatted time as a string
}

// Function to send temperature updates to all connected WebSocket clients
void notifyClients(float temperature)
{
  String message = String(temperature); // Convert temperature to a string
  ws.textAll(message);                  // Send the temperature to all connected WebSocket clients
}

// Function to handle incoming WebSocket messages
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0; // Null-terminate the received data
    Serial.printf("WebSocket message received: %s\n", (char *)data);
  }
}

// Function to handle WebSocket events (e.g., connect, disconnect, data received)
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

// Function to log temperature data to SPIFFS
void logTemperatureToSPIFFS(float temperature)
{
  File file = SPIFFS.open("/temperature_log.txt", FILE_APPEND); // Open the log file in append mode
  if (!file)
  {
    Serial.println("Failed to open log file for writing.");
    return;
  }

  // Create a log entry with the current time and temperature
  String logEntry = getFormattedTime() + " - " + String(temperature) + " °C \n";
  file.print(logEntry); // Write the log entry to the file
  file.close();         // Close the file
}

// Function to generate a CSV file from log data
bool generateCSV(const char *filePath, const char *header, const String &data)
{
  File csvFile = SPIFFS.open(filePath, FILE_WRITE); // Open the CSV file for writing
  if (!csvFile)
  {
    Serial.println("Failed to open CSV file for writing.");
    return false;
  }

  csvFile.println(header); // Write the header row to the CSV file

  // Format the log data by adding extra newlines between entries
  String formattedData = data;
  formattedData.replace("\n", "\n\n");

  csvFile.print(formattedData); // Write the formatted log data to the CSV file
  csvFile.close();              // Close the file
  return true;
}

// Function to generate a CSV file from the temperature log
void generateCSVFromLogs()
{
  File logFile = SPIFFS.open("/temperature_log.txt", FILE_READ); // Open the log file for reading
  if (!logFile)
  {
    Serial.println("Failed to open log file for generating CSV.");
    return;
  }

  // Read the log file contents into a string
  String logs = "";
  while (logFile.available())
  {
    logs += logFile.readStringUntil('\n');
  }
  logFile.close();

  // Generate the CSV file from the log data
  if (!generateCSV("/temperature_log.csv", "Timestamp,Temperature", logs))
  {
    Serial.println("Failed to generate CSV.");
  }
}

void setup()
{
  Serial.begin(115200); // Start serial communication for debugging

  // Configure the reset button pin as an input with an internal pull-up resistor
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Configure the LED pin as an output and ensure it is off initially
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize SPIFFS (file system for storing logs and other files)
  if (!SPIFFS.begin())
  {
    Serial.println("An error occurred while mounting SPIFFS");
    return; // Exit setup if SPIFFS initialization fails
  }

  // Set up Wi-Fi using WiFiManager
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180); // Timeout for configuration portal (in seconds)
  if (!wifiManager.autoConnect("ESP32-Setup", "password"))
  {
    Serial.println("Failed to connect to Wi-Fi and timed out!");
    ESP.restart(); // Restart the ESP32 if Wi-Fi connection fails
  }

  Serial.println("Wi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // Print the assigned IP address

  // Synchronize time using NTP servers
  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov"); // Set timezone to Copenhagen (CET/CEST)
  Serial.print("Waiting for NTP time sync...");
  unsigned long startTime = millis();
  while (!time(nullptr)) // Wait until time is synchronized
  {
    Serial.print(".");
    delay(1000);
    if (millis() - startTime > 30000) // Timeout after 30 seconds
    {
      Serial.println("\nFailed to synchronize time.");
      break;
    }
  }
  Serial.println("\nTime synchronized!");

  // Initialize the DS18B20 temperature sensor
  sensors.begin();
  int deviceCount = 0;
  oneWire.reset_search();
  byte address[8];
  while (oneWire.search(address)) // Search for devices on the OneWire bus
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

  // Set up WebSocket events
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Serve static files from SPIFFS (e.g., HTML, CSS, JavaScript)
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // Add endpoints for various functionalities
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
  static unsigned long buttonPressStart = 0;  // Time when the button was first pressed
  static unsigned long elapsedTime = 0;       // Duration the button has been held
  static unsigned long lastPrintedSecond = 0; // Last second that was printed

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

    // Send temperature updates to WebSocket clients every 5 minutes
    static unsigned long lastTemperatureUpdate = 0;
    if (millis() - lastTemperatureUpdate > 300000)
    {
      notifyClients(temperatureC);          // Notify WebSocket clients
      logTemperatureToSPIFFS(temperatureC); // Log temperature to SPIFFS
      lastTemperatureUpdate = millis();     // Update the last temperature update time
      generateCSVFromLogs();                // Generate CSV from logs
    }
  }

  // Delay for 2 seconds before the next reading
  delay(2000);
}