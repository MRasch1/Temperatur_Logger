#ifndef MEMORY_LOGGER_H
#define MEMORY_LOGGER_H

#include <Arduino.h>

bool generateCSV(const String &filename, const String &header, const String &data);

#endif