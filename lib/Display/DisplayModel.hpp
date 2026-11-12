#pragma once
#include <stdint.h>
#include <string.h>

#include <MaraXProtocol.hpp>

/// @brief Size of the e-paper panel in pixels. The painters draw for exactly this size.
constexpr int16_t kPanelWidth = 400;
constexpr int16_t kPanelHeight = 300;

/**
 * @brief Everything the dashboard shows, apart from the temperature history.
 *
 * Rendering compares two models to decide, whether the panel needs a refresh at all.
 */
struct DisplayModel {
  enum class Wifi : uint8_t { Off, Connecting, Portal, Connected };

  bool hasReading = false;
  MaraXReading reading;
  bool shotRunning = false;
  bool hasLastShot = false;
  uint16_t shotSeconds = 0;   // live value while running, otherwise the last shot
  bool readyForShot = false;  // heat exchanger in the brewing band and steady
  Wifi wifi = Wifi::Off;
  char ip[16] = { 0 };
  const char *apName = nullptr;       // name of the access point while the wifi is being set up
  const char *restartNote = nullptr;  // why the meter restarted, when that was not a normal power on

  /**
   * @brief True, if the dashboard would look the same for both models.
   *
   * Only the values that are drawn count. The firmware version, the boost countdown and the pump field of the
   * reading are not shown and therefore ignored.
   */
  bool sameAs(const DisplayModel &other) const {
    if (hasReading != other.hasReading || shotRunning != other.shotRunning || hasLastShot != other.hasLastShot ||
        readyForShot != other.readyForShot || shotSeconds != other.shotSeconds || wifi != other.wifi ||
        strcmp(ip, other.ip) != 0 || restartNote != other.restartNote) {
      return false;
    }
    if (!hasReading) {
      return true;
    }
    const MaraXReading &theirs = other.reading;
    return reading.mode == theirs.mode && reading.steamTemperature == theirs.steamTemperature &&
           reading.steamTarget == theirs.steamTarget && reading.hxTemperature == theirs.hxTemperature &&
           reading.heatingOn == theirs.heatingOn;
  }
};
