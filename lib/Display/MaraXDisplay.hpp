#pragma once
#include <Arduino.h>
#include <GxEPD2_BW.h>

#include "DashboardPainter.hpp"
#include "DisplayModel.hpp"
#include <TemperatureHistory.hpp>

using PanelDriver = GxEPD2_420;  // Waveshare 4.2" b/w module (GDEW042T2, UC8176 controller)

/**
 * @brief Drives the 400x300 e-paper panel.
 *
 * The whole screen lives in one frame buffer in RAM, so a refresh is one partial update of the panel regardless of
 * how much changed. The panel refresh itself is what takes time (around half a second), so render() skips the
 * refresh entirely, when nothing changed.
 */
class MaraXDisplay {
 public:
  enum class Refresh : uint8_t { None, Partial, Full };

  MaraXDisplay(int16_t csPin, int16_t dcPin, int16_t rstPin, int16_t busyPin);

  /// @param serialDiagBaud Rate for the driver's diagnostics on Serial, 0 disables them.
  void begin(uint32_t serialDiagBaud);

  /// @brief Shows the logo in the variant picked by the seed. Full refresh.
  void showBootScreen(uint32_t seed);

  /**
   * @brief Draws the dashboard and refreshes the panel, when something changed.
   *
   * @param forceFull Use a full refresh (slow, flickers, removes ghosting) instead of a partial one.
   * @param elapsedSeconds Seconds since the session started, defines the visible time window of the graph.
   * @return Which kind of refresh happened.
   */
  Refresh render(const DisplayModel &model, const TemperatureHistory &history, uint32_t elapsedSeconds, bool forceFull);

  /**
   * @brief Clears the panel, shows the off screen and puts the panel into deep sleep.
   *
   * To be called before the power is lost. The clearing is two full refreshes to white, which wipe the ghosts of
   * the dashboard, the logo (in the variant picked by the seed) is a third one: about five seconds in total.
   */
  void shutdown(uint32_t seed);

 private:
  void showLogo(uint32_t seed, const char *tag);

  GxEPD2_BW<PanelDriver, PanelDriver::HEIGHT> display_;
  DashboardPainter painter_;
  DisplayModel lastModel_;
  bool hasRendered_ = false;
  uint32_t lastHistoryLatestSeconds_ = 0;
  uint8_t lastShotCount_ = 0;
  uint32_t lastWindowStartSeconds_ = 0;
  uint32_t lastWindowSpanSeconds_ = 0;
};

static_assert(PanelDriver::WIDTH == kPanelWidth && PanelDriver::HEIGHT == kPanelHeight,
              "the painters are made for a 400x300 panel");
