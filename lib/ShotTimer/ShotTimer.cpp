#include "ShotTimer.hpp"

namespace {

/// @brief Signed difference of two millis() stamps, so a stamp slightly in the future does not wrap to a huge value.
int32_t since(uint32_t fromMs, uint32_t nowMs) { return static_cast<int32_t>(nowMs - fromMs); }

}  // namespace

ShotTimer::ShotTimer(uint32_t stopThresholdMs, uint32_t minShotMs)
    : stopThresholdMs_(stopThresholdMs), minShotMs_(minShotMs) {}

void ShotTimer::noteActivity(uint32_t atMs) {
  if (!running_) {
    running_ = true;
    startedAt_ = atMs;
    startedEvent_ = true;
  }
  lastActivityAt_ = atMs;
}

void ShotTimer::update(uint32_t nowMs) {
  if (!running_) {
    return;
  }
  if (since(lastActivityAt_, nowMs) <= static_cast<int32_t>(stopThresholdMs_)) {
    return;
  }
  running_ = false;
  stoppedEvent_ = true;
  const uint32_t duration = lastActivityAt_ - startedAt_;
  lastRunMs_ = duration;
  if (duration >= minShotMs_) {
    hasLastShot_ = true;
    lastShotMs_ = duration;
    lastShotStartedAt_ = startedAt_;
    shotFinishedEvent_ = true;
  }
}

uint32_t ShotTimer::elapsedMs(uint32_t nowMs) const {
  if (running_) {
    const int32_t elapsed = since(startedAt_, nowMs);
    return elapsed > 0 ? static_cast<uint32_t>(elapsed) : 0;
  }
  return hasLastShot_ ? lastShotMs_ : 0;
}

bool ShotTimer::takeStartedEvent() {
  const bool event = startedEvent_;
  startedEvent_ = false;
  return event;
}

bool ShotTimer::takeStoppedEvent() {
  const bool event = stoppedEvent_;
  stoppedEvent_ = false;
  return event;
}

bool ShotTimer::takeShotFinishedEvent() {
  const bool event = shotFinishedEvent_;
  shotFinishedEvent_ = false;
  return event;
}
