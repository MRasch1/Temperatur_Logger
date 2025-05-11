#include <FS.h>       // Include the file system library for file operations
#include <SPIFFS.h>   // Include the SPI Flash File System library
#include <Arduino.h>  // Include the Arduino core library for Serial and String utilities


bool generateCSV(const String &filename, const String &header, const String &data) {
    // Attempt to mount the SPIFFS file system
    if (!SPIFFS.begin()) {
        Serial.println("Failed to mount SPIFFS."); // Log an error if mounting fails
        return false; // Return false to indicate failure
    }

    // Open the specified file in write mode
    File file = SPIFFS.open(filename, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing."); // Log an error if the file cannot be opened
        return false; // Return false to indicate failure
    }

    // Write the header row to the file
    file.println(header);

    // Write the data to the file
    file.println(data);

    // Close the file to ensure all data is saved
    file.close();

    // Log a success message to the Serial monitor
    Serial.println("CSV file generated successfully.");
    return true; // Return true to indicate success
}