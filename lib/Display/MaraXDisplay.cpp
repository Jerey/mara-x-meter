#include "MaraXDisplay.hpp"

#include "LogoScreen.hpp"

MaraXDisplay::MaraXDisplay(int16_t csPin, int16_t dcPin, int16_t rstPin, int16_t busyPin)
    : display_(PanelDriver(csPin, dcPin, rstPin, busyPin)), painter_(display_, GxEPD_BLACK, GxEPD_WHITE) {}

void MaraXDisplay::begin(uint32_t serialDiagBaud) {
  display_.init(serialDiagBaud, true, 10, false);  // 10 ms reset pulse, as the previous library used
  display_.setRotation(0);
  display_.setTextWrap(false);
  display_.setTextColor(GxEPD_BLACK);
}

void MaraXDisplay::showBootScreen(uint32_t seed) { showLogo(seed, "SYSTEM ONLINE"); }

void MaraXDisplay::showLogo(uint32_t seed, const char *tag) {
  display_.setFullWindow();
  display_.fillScreen(GxEPD_WHITE);
  LogoScreen::paint(display_, GxEPD_BLACK, GxEPD_WHITE, seed, tag);
  display_.display(false);
}

MaraXDisplay::Refresh MaraXDisplay::render(const DisplayModel &model, const TemperatureHistory &history,
                                           uint32_t elapsedSeconds, bool forceFull) {
  const GraphWindow window = GraphWindow::forElapsed(elapsedSeconds);
  const bool windowChanged =
      window.startSeconds != lastWindowStartSeconds_ || window.spanSeconds != lastWindowSpanSeconds_;
  // The newest sample time moves on even when the ring buffer is full and the size stays the same.
  const bool historyChanged =
      history.latestSeconds() != lastHistoryLatestSeconds_ || history.shotCount() != lastShotCount_;
  const bool modelChanged = !hasRendered_ || !model.sameAs(lastModel_);
  if (!forceFull && !windowChanged && !historyChanged && !modelChanged) {
    return Refresh::None;
  }
  // A new time scale redraws every trace at a new position: use a full refresh, so no ghost of the old one stays.
  const bool full = forceFull || !hasRendered_ || windowChanged;

  display_.setFullWindow();
  display_.fillScreen(GxEPD_WHITE);
  painter_.paint(model, history, window);
  display_.display(!full);

  lastModel_ = model;
  hasRendered_ = true;
  lastHistoryLatestSeconds_ = history.latestSeconds();
  lastShotCount_ = history.shotCount();
  lastWindowStartSeconds_ = window.startSeconds;
  lastWindowSpanSeconds_ = window.spanSeconds;
  return full ? Refresh::Full : Refresh::Partial;
}

void MaraXDisplay::shutdown(uint32_t seed) {
  // One white pass leaves faint ghosts of the dashboard, two get rid of them.
  display_.setFullWindow();
  display_.fillScreen(GxEPD_WHITE);
  for (int pass = 0; pass < 2; ++pass) {
    display_.display(false);
  }
  showLogo(seed, "SYSTEM OFFLINE");
  display_.hibernate();
}
