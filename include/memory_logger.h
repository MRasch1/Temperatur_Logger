#ifndef MEMORY_LOGGER_H
#define MEMORY_LOGGER_H

#include <Arduino.h> // Include the Arduino core library for String and other utilities

// Function declaration for generating a CSV file
// Parameters:
// - filename: The name of the CSV file to be created or written to
// - header: The header row for the CSV file (e.g., column names)
// - data: The data to be written to the CSV file, formatted as a string
// Returns:
// - true if the CSV file was successfully created or written to
// - false if there was an error during the process
bool generateCSV(const String &filename, const String &header, const String &data);

#endif // MEMORY_LOGGER_H