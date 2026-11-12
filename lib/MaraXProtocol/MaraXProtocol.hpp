#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @brief One reading of the serial telemetry, which the Lelit Mara X sends roughly twice a second.
 *
 * Example line: "C1.06,116,124,093,0840,1,0"
 *   0  mode and firmware: 'C' = coffee priority, 'V' = steam ("vapore") priority, then the firmware version
 *   1  steam boiler temperature in C
 *   2  steam boiler target temperature in C
 *   3  heat exchanger temperature in C
 *   4  countdown in s of the fast heating phase: starts at 1200 after switching on and after a shot, counts down
 *      once a second and jumps to 0 once the target temperature is reached
 *   5  heating element on (1) or off (0)
 *   6  pump running (1) or off (0), only present on newer firmware
 */
struct MaraXReading {
  enum class Mode : uint8_t { Unknown, CoffeePriority, SteamPriority };

  Mode mode = Mode::Unknown;
  char firmware[8] = { 0 };
  uint8_t steamTemperature = 0;
  uint8_t steamTarget = 0;
  uint8_t hxTemperature = 0;
  uint16_t boostCountdown = 0;
  bool heatingOn = false;
  bool hasPumpField = false;
  bool pumpOn = false;
};

/**
 * @brief Parses one complete line (without the trailing newline).
 *
 * @return false, when the line does not look like a Mara X message. The output is only modified on success.
 */
bool parseMaraXLine(const char *line, MaraXReading &out);

/**
 * @brief Collects single characters into complete lines. Lines, that overflow the buffer, are dropped.
 */
class LineReader {
 public:
  static constexpr size_t kCapacity = 48;

  /// @brief Feeds one character. @return true, when a complete line is available via line().
  bool push(char c);

  /// @brief The last complete line, valid until the next push().
  const char *line() const { return buffer_; }

 private:
  char buffer_[kCapacity] = { 0 };
  size_t length_ = 0;
  bool overflowed_ = false;
};
