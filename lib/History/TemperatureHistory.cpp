#include "TemperatureHistory.hpp"

bool TemperatureHistory::add(uint32_t elapsedSeconds, uint8_t hx, uint8_t steam) {
  const uint32_t slot = elapsedSeconds / kSampleIntervalSeconds;
  if (hasCurrent_ && slot == currentSlot_) {
    // Same interval: overwrite the newest sample.
    const uint16_t newest = (head_ + kCapacity - 1) % kCapacity;
    samples_[newest] = Sample{ hx, steam };
    return false;
  }
  if (!hasCurrent_) {
    firstSlot_ = slot;
  }
  // Fill skipped intervals (e.g. after a long display refresh) with the current value so the line stays continuous.
  uint32_t fillSlot = hasCurrent_ ? currentSlot_ + 1 : slot;
  for (; fillSlot <= slot; ++fillSlot) {
    samples_[head_] = Sample{ hx, steam };
    head_ = (head_ + 1) % kCapacity;
    if (size_ < kCapacity) {
      ++size_;
    } else {
      ++firstSlot_;
    }
  }
  hasCurrent_ = true;
  currentSlot_ = slot;
  return true;
}

void TemperatureHistory::addShot(uint32_t startSeconds, uint16_t durationSeconds) {
  if (shotCount_ == kMaxShots) {
    for (uint8_t i = 1; i < kMaxShots; ++i) {
      shots_[i - 1] = shots_[i];
    }
    --shotCount_;
  }
  shots_[shotCount_++] = Shot{ startSeconds, durationSeconds };
}

TemperatureHistory::Sample TemperatureHistory::at(uint16_t index) const {
  const uint16_t oldest = (head_ + kCapacity - size_) % kCapacity;
  return samples_[(oldest + index) % kCapacity];
}

bool TemperatureHistory::hxSteady(uint16_t forSeconds, uint8_t minHx, uint8_t maxHx, uint8_t maxSpread) const {
  const uint16_t needed = forSeconds / kSampleIntervalSeconds + 1;
  if (size_ < needed) {
    return false;
  }
  uint8_t lowest = 255;
  uint8_t highest = 0;
  for (uint16_t i = size_ - needed; i < size_; ++i) {
    const uint8_t hx = at(i).hx;
    if (hx < minHx || hx > maxHx) {
      return false;
    }
    lowest = hx < lowest ? hx : lowest;
    highest = hx > highest ? hx : highest;
  }
  return highest - lowest <= maxSpread;
}

void TemperatureHistory::clear() {
  head_ = 0;
  size_ = 0;
  firstSlot_ = 0;
  hasCurrent_ = false;
  shotCount_ = 0;
}

GraphWindow GraphWindow::forElapsed(uint32_t elapsedSeconds) {
  struct Step {
    uint16_t spanMin;
    uint16_t tickMin;
  };
  static constexpr Step kSteps[] = { { 30, 5 }, { 45, 5 }, { 60, 10 }, { 90, 15 }, { 120, 20 }, { 180, 30 } };
  static constexpr size_t kStepCount = sizeof(kSteps) / sizeof(kSteps[0]);

  for (size_t i = 0; i < kStepCount; ++i) {
    const uint32_t spanSeconds = kSteps[i].spanMin * 60u;
    if (elapsedSeconds <= spanSeconds) {
      return GraphWindow{ 0, spanSeconds, static_cast<uint16_t>(kSteps[i].tickMin * 60u) };
    }
  }
  // Longer than the largest step: scroll, keeping the left edge on a tick so the labels stay readable.
  const Step &last = kSteps[kStepCount - 1];
  const uint32_t spanSeconds = last.spanMin * 60u;
  const uint32_t tickSeconds = last.tickMin * 60u;
  const uint32_t rawStart = elapsedSeconds - spanSeconds;
  const uint32_t start =
      ((rawStart + tickSeconds - 1) / tickSeconds) * tickSeconds;  // round up so "now" is inside the window
  return GraphWindow{ start, spanSeconds, static_cast<uint16_t>(tickSeconds) };
}

int16_t GraphWindow::xFor(uint32_t timeSeconds, int16_t x0, int16_t width) const {
  if (timeSeconds <= startSeconds) {
    return x0;
  }
  const uint32_t offset = timeSeconds - startSeconds;
  if (offset >= spanSeconds) {
    return x0 + width - 1;
  }
  // offset is below three hours in seconds and width below 400, so the product fits into 32 bits easily.
  return x0 + static_cast<int16_t>((offset * static_cast<uint32_t>(width - 1)) / spanSeconds);
}

int16_t yForTemperature(int temperature, int16_t y0, int16_t height, int minTemp, int maxTemp) {
  const int16_t yBottom = y0 + height - 1;
  if (temperature <= minTemp) {
    return yBottom;
  }
  if (temperature >= maxTemp) {
    return y0;
  }
  return yBottom -
         static_cast<int16_t>((static_cast<int32_t>(temperature - minTemp) * (height - 1)) / (maxTemp - minTemp));
}
