#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Ring buffer of the temperatures plus the shots that happened, used to draw the graph.
 *
 * One sample every kSampleIntervalS for up to kCapacity samples (three hours). Memory: kCapacity * 2 bytes.
 * Older samples are dropped once the buffer is full, so the graph scrolls after three hours.
 */
class TemperatureHistory {
 public:
  static constexpr uint16_t kSampleIntervalS = 5;
  static constexpr uint16_t kCapacity = 3 * 60 * 60 / kSampleIntervalS;
  static constexpr uint8_t kMaxShots = 32;

  struct Sample {
    uint8_t hx;
    uint8_t steam;
  };

  struct Shot {
    uint32_t startS;
    uint16_t durationS;
  };

  /**
   * @brief Records the current temperatures.
   *
   * Values within the same sample interval overwrite each other, so the latest reading of an interval is kept.
   * @return true, when a new sample slot was started, i.e. the graph changed.
   */
  bool add(uint32_t elapsedS, uint8_t hx, uint8_t steam);

  /// @brief Remembers a finished shot for the markers in the graph. The oldest marker is dropped when full.
  void addShot(uint32_t startS, uint16_t durationS);

  uint16_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  /// @brief Sample by age: index 0 is the oldest kept sample.
  Sample at(uint16_t index) const;

  /// @brief Elapsed seconds (aligned to the sample interval) of the sample at the given index.
  uint32_t timeAt(uint16_t index) const { return (firstSlot_ + index) * kSampleIntervalS; }

  /// @brief Elapsed seconds of the newest sample, 0 when empty.
  uint32_t latestS() const { return empty() ? 0 : timeAt(size_ - 1); }

  /**
   * @brief True, if the heat exchanger stayed within the band and moved at most maxSpread over the last forS seconds.
   *
   * The machine is ready for a shot then. Needs at least forS seconds of samples.
   */
  bool hxSteady(uint16_t forS, uint8_t minHx, uint8_t maxHx, uint8_t maxSpread) const;

  uint8_t shotCount() const { return shotCount_; }
  Shot shot(uint8_t index) const { return shots_[index]; }

  void clear();

 private:
  Sample samples_[kCapacity] = {};
  uint16_t head_ = 0;       // slot where the next sample will be written
  uint16_t size_ = 0;       // number of valid samples
  uint32_t firstSlot_ = 0;  // absolute slot number (elapsedS / interval) of the oldest sample
  bool hasCurrent_ = false;
  uint32_t currentSlot_ = 0;

  Shot shots_[kMaxShots] = {};
  uint8_t shotCount_ = 0;
};

/**
 * @brief The time span shown on the x axis.
 *
 * The graph starts with 30 minutes and widens in steps, so the whole session stays visible. Beyond the largest step
 * the graph scrolls.
 */
struct GraphWindow {
  uint32_t startS;  // time at the left edge
  uint32_t spanS;   // visible span
  uint16_t tickS;   // distance between labelled ticks on the x axis

  static GraphWindow forElapsed(uint32_t elapsedS);

  /// @brief x position for a time stamp, clamped to the graph area.
  int16_t xFor(uint32_t timeS, int16_t x0, int16_t width) const;
};

/// @brief y position for a temperature in an area spanning [y0, y0 + height) for temperatures [minTemp, maxTemp].
int16_t yForTemperature(int temperature, int16_t y0, int16_t height, int minTemp, int maxTemp);
