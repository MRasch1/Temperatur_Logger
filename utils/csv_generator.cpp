#include <FS.h>
#include <SPIFFS.h>
#include <Arduino.h>

bool generateCSV(const String &filename, const String &header, const String &data) {
    if (!SPIFFS.begin()) {
        Serial.println("Failed to mount SPIFFS.");
        return false;
    }

    File file = SPIFFS.open(filename, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing.");
        return false;
    }

    file.println(header); // Write the header
    file.println(data);   // Write the data
    file.close();

    Serial.println("CSV file generated successfully.");
    return true;
}