#pragma once
#include <stdint.h>

/**
 * @brief Detects, that the power was cut, from the supply voltage.
 *
 * The meter hangs on a wall socket, which is switched off after the coffee. A supercapacitor keeps the D1 mini
 * alive for a few seconds, which is enough to put the display to sleep properly. The supply voltage drops slowly
 * on the capacitor, so a drop below the highest voltage seen so far is the sign for the power cut. A few readings
 * in a row have to agree, both for the highest voltage and for the drop, so a single noisy reading neither lifts the
 * reference nor switches off the display.
 *
 * After the shutdown the supply usually dies. If it comes back instead (outlet switched on again quickly, or the
 * drop was not a power cut after all), the meter should start over instead of staying dark until the next real
 * power cycle. On a dying capacitor the voltage only goes down, so a voltage, that rises again, means the power is
 * back, no matter where the reference was.
 *
 * The caller reads the voltage and passes it in.
 */
class PowerMonitor {
 public:
  /**
   * @param brownoutMillivolt Drop below the highest voltage seen, which means the power was cut.
   * @param confirmations Number of readings in a row that have to show the drop (or a new highest voltage).
   * @param recoveryMs How long the supply has to be back after a shutdown to count as recovered.
   */
  PowerMonitor(uint16_t brownoutMillivolt, uint8_t confirmations, uint32_t recoveryMs);

  /// @brief Feeds a reading while the meter runs. Returns true once the power cut is confirmed.
  bool noteSupply(uint16_t millivolt);

  /**
   * @brief Feeds a reading after the shutdown. Returns true once the supply is back for recoveryMs.
   *
   * Back means: close to the highest voltage seen before, or clearly above the lowest voltage seen since the
   * shutdown, i.e. rising again.
   */
  bool noteSupplyAfterShutdown(uint16_t millivolt, uint32_t nowMs);

  uint16_t maxMillivolt() const { return maxMillivolt_; }

 private:
  const uint16_t brownoutMillivolt_;
  const uint8_t confirmations_;
  const uint32_t recoveryMs_;
  uint16_t maxMillivolt_ = 0;
  uint16_t candidateMax_ = 0;
  uint8_t highReadings_ = 0;
  uint8_t lowReadings_ = 0;
  uint16_t minSinceShutdown_ = 0xFFFF;
  bool recovering_ = false;
  uint32_t recoveringSinceMs_ = 0;
};
