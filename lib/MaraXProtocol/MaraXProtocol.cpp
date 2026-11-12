#include "MaraXProtocol.hpp"

#include <stdlib.h>
#include <string.h>

namespace {

constexpr size_t kMaxFields = 8;

/// @brief Splits the line at commas into at most kMaxFields fields. @return The number of fields found.
size_t splitFields(char *line, const char *fields[kMaxFields]) {
  size_t count = 0;
  char *cursor = line;
  while (count < kMaxFields) {
    fields[count++] = cursor;
    char *comma = strchr(cursor, ',');
    if (comma == nullptr) {
      break;
    }
    *comma = '\0';
    cursor = comma + 1;
  }
  return count;
}

/// @brief Parses an unsigned decimal field. @return false for empty fields or anything other than digits.
bool parseUnsigned(const char *field, unsigned long &value) {
  if (*field == '\0') {
    return false;
  }
  for (const char *cursor = field; *cursor != '\0'; ++cursor) {
    if (*cursor < '0' || *cursor > '9') {
      return false;
    }
  }
  value = strtoul(field, nullptr, 10);
  return true;
}

bool parseTemperature(const char *field, uint8_t &out) {
  unsigned long value = 0;
  if (!parseUnsigned(field, value) || value > 255) {
    return false;
  }
  out = static_cast<uint8_t>(value);
  return true;
}

}  // namespace

bool parseMaraXLine(const char *line, MaraXReading &out) {
  if (line == nullptr) {
    return false;
  }
  while (*line == ' ' || *line == '\r' || *line == '\n') {
    ++line;
  }
  char copy[LineReader::kCapacity];
  strncpy(copy, line, sizeof(copy) - 1);
  copy[sizeof(copy) - 1] = '\0';
  // Strip trailing line endings or spaces.
  for (size_t length = strlen(copy);
       length > 0 && (copy[length - 1] == '\r' || copy[length - 1] == '\n' || copy[length - 1] == ' '); --length) {
    copy[length - 1] = '\0';
  }

  const char *fields[kMaxFields] = { nullptr };
  const size_t fieldCount = splitFields(copy, fields);
  if (fieldCount < 6) {
    return false;
  }

  MaraXReading reading;
  switch (fields[0][0]) {
    case 'C': reading.mode = MaraXReading::Mode::CoffeePriority; break;
    case 'V': reading.mode = MaraXReading::Mode::SteamPriority; break;
    default: return false;
  }
  strncpy(reading.firmware, fields[0] + 1, sizeof(reading.firmware) - 1);

  unsigned long boost = 0;
  unsigned long heating = 0;
  if (!parseTemperature(fields[1], reading.steamTemperature) || !parseTemperature(fields[2], reading.steamTarget) ||
      !parseTemperature(fields[3], reading.hxTemperature) || !parseUnsigned(fields[4], boost) ||
      !parseUnsigned(fields[5], heating)) {
    return false;
  }
  reading.boostCountdown = static_cast<uint16_t>(boost > 65535 ? 65535 : boost);
  reading.heatingOn = heating != 0;

  if (fieldCount >= 7) {
    unsigned long pump = 0;
    if (parseUnsigned(fields[6], pump)) {
      reading.hasPumpField = true;
      reading.pumpOn = pump != 0;
    }
  }

  out = reading;
  return true;
}

bool LineReader::push(char character) {
  if (character == '\n') {
    const bool complete = !overflowed_ && length_ > 0;
    buffer_[length_] = '\0';
    length_ = 0;
    overflowed_ = false;
    return complete;
  }
  if (character == '\r') {
    return false;
  }
  if (length_ >= kCapacity - 1) {
    overflowed_ = true;
    return false;
  }
  buffer_[length_++] = character;
  return false;
}
