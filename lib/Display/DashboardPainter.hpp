#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

#include "DisplayModel.hpp"
#include <TemperatureHistory.hpp>

/**
 * @brief Draws the dashboard onto any Adafruit GFX canvas.
 *
 * On the device that is the e-paper frame buffer, in the host side preview (tools/preview) a bitmap canvas. The
 * painter knows nothing about the panel itself.
 */
class DashboardPainter {
 public:
  DashboardPainter(Adafruit_GFX &gfx, uint16_t black, uint16_t white);

  /// @brief Paints the whole dashboard. The canvas is expected to be cleared to white.
  void paint(const DisplayModel &model, const TemperatureHistory &history, const GraphWindow &window);

 private:
  void drawHeader(const DisplayModel &model);
  void drawBadges(const DisplayModel &model);
  void drawGraph(const TemperatureHistory &history, const GraphWindow &window);
  void drawTraces(const TemperatureHistory &history, const GraphWindow &window);
  void drawShotMarkers(const TemperatureHistory &history, const GraphWindow &window);
  void drawLegend();
  void drawWaitingNote(const DisplayModel &model);

  void drawTemperatureValue(int16_t x, int16_t baseline, int temperature, bool valid, const GFXfont &font);
  int16_t drawBadge(int16_t x, int16_t y, const char *text, bool filled);
  void drawTextCentered(int16_t xCenter, int16_t baseline, const char *text);
  void drawTextRightAligned(int16_t xRight, int16_t baseline, const char *text);
  int16_t textWidth(const char *text);

  Adafruit_GFX &gfx_;
  const uint16_t black_;
  const uint16_t white_;
};
