#include "PowerMonitor.hpp"

PowerMonitor::PowerMonitor(uint16_t brownoutMillivolt, uint8_t confirmations, uint32_t recoveryMs)
    : brownoutMillivolt_(brownoutMillivolt), confirmations_(confirmations), recoveryMs_(recoveryMs) {}

bool PowerMonitor::noteSupply(uint16_t millivolt) {
  // The reference only rises, when several readings in a row are above it. A single noisy reading would
  // otherwise lift the reference and make every normal reading afterwards look like a brownout.
  if (millivolt > maxMillivolt_) {
    candidateMax_ = highReadings_ == 0 ? millivolt : (millivolt < candidateMax_ ? millivolt : candidateMax_);
    if (++highReadings_ >= confirmations_) {
      maxMillivolt_ = candidateMax_;
      highReadings_ = 0;
    }
    lowReadings_ = 0;  // above the reference is never a brownout, also before the first reference is known
    return false;
  }
  highReadings_ = 0;
  if (maxMillivolt_ - millivolt > brownoutMillivolt_) {
    ++lowReadings_;
  } else {
    lowReadings_ = 0;
  }
  return lowReadings_ >= confirmations_;
}

bool PowerMonitor::noteSupplyAfterShutdown(uint16_t millivolt, uint32_t nowMs) {
  if (millivolt < minSinceShutdown_) {
    minSinceShutdown_ = millivolt;
  }
  // Either close to what it was before the shutdown, or clearly up again from the lowest point: the capacitor
  // alone never makes the voltage rise.
  const bool nearReference = maxMillivolt_ - millivolt <= brownoutMillivolt_ / 2;
  const bool risingAgain = millivolt - minSinceShutdown_ >= brownoutMillivolt_ / 2;
  if (!nearReference && !risingAgain) {
    recovering_ = false;
    return false;
  }
  if (!recovering_) {
    recovering_ = true;
    recoveringSinceMs_ = nowMs;
    return false;
  }
  return nowMs - recoveringSinceMs_ >= recoveryMs_;
}
