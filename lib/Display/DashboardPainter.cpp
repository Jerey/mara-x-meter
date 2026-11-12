#include "DashboardPainter.hpp"

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <stdio.h>
#include <string.h>

namespace {

// ---- Layout (400 x 300) ----
constexpr int16_t kGap = 6;  // Spacing between the elements.

// Header with three tiles: heat exchanger, steam boiler, shot timer.
constexpr int16_t kHeaderHeight = 58;
constexpr int16_t kTileLabelBaseline = 14;
constexpr int16_t kTileValueBaseline = 52;
constexpr int16_t kHxTileX = 0;
constexpr int16_t kSteamTileX = 150;
constexpr int16_t kShotTileX = 288;
constexpr int16_t kSmallFontCapHeight = 13;  // FreeSans 9pt
constexpr int16_t kSmallFontHeight = 8;      // built in 6x8 font

// Strip of badges below the tiles: ready, mode, wifi.
constexpr int16_t kBadgeY = kHeaderHeight + 4;
constexpr int16_t kBadgeHeight = 18;
constexpr int16_t kBadgePadX = 5;

// Graph area.
constexpr int16_t kGraphX = 34;
constexpr int16_t kGraphY = kBadgeY + kBadgeHeight + 4;
constexpr int16_t kGraphWidth = kPanelWidth - kGraphX - kGap;
constexpr int16_t kAxisLabelTop = kPanelHeight - kSmallFontHeight - 2;
constexpr int16_t kGraphHeight = kAxisLabelTop - 5 - kGraphY;
constexpr int16_t kMarkerHalfWidth = 4;
constexpr int16_t kMarkerHeight = 6;

constexpr int kMinTemp = 20;
constexpr int kMaxTemp = 150;
constexpr int kGridStep = 20;

constexpr int16_t graphBottom() { return kGraphY + kGraphHeight - 1; }

// Icons, 1 bit per pixel, most significant bit first, 1 = ink.
constexpr int16_t kFlameIconWidth = 11;
constexpr int16_t kFlameIconHeight = 14;
const uint8_t kFlameIcon[] PROGMEM = {
  0x04, 0x00, 0x0C, 0x00, 0x0E, 0x00, 0x1E, 0x00, 0x1F, 0x00, 0x3F, 0x00, 0x3F, 0x80,
  0x7B, 0xC0, 0x71, 0xC0, 0xF1, 0xE0, 0xF1, 0xE0, 0x71, 0xC0, 0x31, 0x80, 0x1F, 0x00,
};
constexpr int16_t kWifiIconWidth = 15;
constexpr int16_t kWifiIconHeight = 11;
const uint8_t kWifiIcon[] PROGMEM = {
  0x07, 0xC0, 0x18, 0x30, 0x27, 0xC8, 0x4C, 0x64, 0x90, 0x12, 0x23,
  0x88, 0x44, 0x44, 0x08, 0x20, 0x01, 0x00, 0x03, 0x80, 0x01, 0x00,
};

}  // namespace

DashboardPainter::DashboardPainter(Adafruit_GFX &gfx, uint16_t black, uint16_t white)
    : gfx_(gfx), black_(black), white_(white) {}

void DashboardPainter::paint(const DisplayModel &model, const TemperatureHistory &history, const GraphWindow &window) {
  gfx_.setTextWrap(false);
  gfx_.setTextColor(black_);
  drawHeader(model);
  drawBadges(model);
  drawGraph(history, window);
  if (history.empty()) {
    drawWaitingNote(model);
  }
}

// ----------------------------------------------------------------------------------------------------------------
// Header
// ----------------------------------------------------------------------------------------------------------------

void DashboardPainter::drawHeader(const DisplayModel &model) {
  const bool valid = model.hasReading;
  const MaraXReading &r = model.reading;

  gfx_.setFont(&FreeSans9pt7b);
  gfx_.setCursor(kHxTileX + kGap, kTileLabelBaseline);
  gfx_.print("HX");
  gfx_.setCursor(kSteamTileX + kGap, kTileLabelBaseline);
  gfx_.print("STEAM");
  if (valid && r.heatingOn) {
    // A small flame next to the label while the heating element is on.
    gfx_.drawBitmap(gfx_.getCursorX() + kGap, kTileLabelBaseline - kFlameIconHeight + 1, kFlameIcon, kFlameIconWidth,
                    kFlameIconHeight, black_);
  }
  gfx_.setCursor(kShotTileX + kGap, kTileLabelBaseline);
  gfx_.print(!model.shotRunning && model.hasLastShot ? "LAST SHOT" : "SHOT");

  // Heat exchanger: the number that matters most, so it is the largest.
  drawTemperatureValue(kHxTileX + kGap, kTileValueBaseline, r.hxTemperature, valid, FreeSansBold24pt7b);

  // Steam boiler: a bit smaller, the target behind a slash.
  char text[16];
  gfx_.setFont(&FreeSansBold18pt7b);
  if (valid) {
    snprintf(text, sizeof(text), "%d", r.steamTemperature);
  } else {
    strncpy(text, "--", sizeof(text));
  }
  gfx_.setCursor(kSteamTileX + kGap, kTileValueBaseline);
  gfx_.print(text);
  const int16_t afterSteam = gfx_.getCursorX() + 3;
  gfx_.setFont(&FreeSansBold12pt7b);
  if (valid) {
    snprintf(text, sizeof(text), "/%d", r.steamTarget);
  } else {
    strncpy(text, "/--", sizeof(text));
  }
  gfx_.setCursor(afterSteam, kTileValueBaseline);
  gfx_.print(text);

  // Shot timer: seconds, large. Shows the last shot after the pump stopped.
  gfx_.setFont(&FreeSansBold24pt7b);
  if (model.shotRunning || model.hasLastShot) {
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(model.shotSeconds));
  } else {
    strncpy(text, "--", sizeof(text));
  }
  gfx_.setCursor(kShotTileX + kGap, kTileValueBaseline);
  gfx_.print(text);
  gfx_.setFont(&FreeSans9pt7b);
  gfx_.setCursor(gfx_.getCursorX() + 3, kTileValueBaseline);
  gfx_.print("s");
  if (model.shotRunning) {
    // A filled dot shows, that the timer is live.
    gfx_.fillCircle(kPanelWidth - 10, kTileLabelBaseline - 4, 4, black_);
  }

  // Tile separators.
  gfx_.drawFastVLine(kSteamTileX - 1, 4, kHeaderHeight - 8, black_);
  gfx_.drawFastVLine(kShotTileX - 1, 4, kHeaderHeight - 8, black_);
}

void DashboardPainter::drawTemperatureValue(int16_t x, int16_t baseline, int temperature, bool valid,
                                            const GFXfont &font) {
  char text[8];
  if (valid) {
    snprintf(text, sizeof(text), "%d", temperature);
  } else {
    strncpy(text, "--", sizeof(text));
  }
  gfx_.setFont(&font);
  gfx_.setCursor(x, baseline);
  gfx_.print(text);
  // Degree sign and unit in the small font on the same baseline. The GFX fonts have no degree glyph, so it is a
  // small ring at the top of the "C".
  const int16_t xUnit = gfx_.getCursorX() + 4;
  gfx_.drawCircle(xUnit + 3, baseline - kSmallFontCapHeight + 2, 3, black_);
  gfx_.setFont(&FreeSans9pt7b);
  gfx_.setCursor(xUnit + 9, baseline);
  gfx_.print("C");
}

// ----------------------------------------------------------------------------------------------------------------
// Badges
// ----------------------------------------------------------------------------------------------------------------

void DashboardPainter::drawBadges(const DisplayModel &model) {
  gfx_.setFont(&FreeSans9pt7b);
  int16_t x = kGap;
  if (model.hasReading) {
    const MaraXReading &r = model.reading;
    if (model.readyForShot) {
      x = drawBadge(x, kBadgeY, "READY", true) + kGap;
    }
    // Coffee priority is the normal case and gets no badge.
    if (r.mode == MaraXReading::Mode::SteamPriority) {
      x = drawBadge(x, kBadgeY, "STEAM PRIO", true) + kGap;
    }
  }

  // Wifi state right aligned: the arcs, crossed out without wifi, and the IP in the small font for flashing.
  const char *wifi = "";
  switch (model.wifi) {
    case DisplayModel::Wifi::Connected: wifi = model.ip; break;
    case DisplayModel::Wifi::Connecting: wifi = "..."; break;
    case DisplayModel::Wifi::Portal: wifi = "SETUP"; break;
    case DisplayModel::Wifi::Off:
    default: break;
  }
  gfx_.setFont(nullptr);
  gfx_.setTextColor(black_);
  const int16_t lineY = model.restartNote ? kBadgeY : kBadgeY + (kBadgeHeight - kSmallFontHeight) / 2;
  const int16_t textWidthPx = *wifi ? textWidth(wifi) + 4 : 0;
  const int16_t iconX = kPanelWidth - kGap - textWidthPx - kWifiIconWidth;
  const int16_t iconY = lineY + kSmallFontHeight / 2 - kWifiIconHeight / 2;
  gfx_.drawBitmap(iconX, iconY, kWifiIcon, kWifiIconWidth, kWifiIconHeight, black_);
  if (model.wifi == DisplayModel::Wifi::Off) {
    gfx_.drawLine(iconX, iconY + kWifiIconHeight - 1, iconX + kWifiIconWidth - 1, iconY, black_);
    gfx_.drawLine(iconX + 1, iconY + kWifiIconHeight - 1, iconX + kWifiIconWidth, iconY, black_);
  }
  if (*wifi) {
    drawTextRightAligned(kPanelWidth - kGap, lineY, wifi);
  }
  if (model.restartNote) {
    // The meter did not start from a power on: say why, in a second line below the wifi state, until the next
    // proper power cycle.
    char note[40];
    snprintf(note, sizeof(note), "RESET: %s", model.restartNote);
    drawTextRightAligned(kPanelWidth - kGap, kBadgeY + kSmallFontHeight + 2, note);
  }
}

int16_t DashboardPainter::drawBadge(int16_t x, int16_t y, const char *text, bool filled) {
  const int16_t width = textWidth(text) + 2 * kBadgePadX;
  if (filled) {
    gfx_.fillRoundRect(x, y, width, kBadgeHeight, 3, black_);
    gfx_.setTextColor(white_);
  } else {
    gfx_.drawRoundRect(x, y, width, kBadgeHeight, 3, black_);
    gfx_.setTextColor(black_);
  }
  gfx_.setCursor(x + kBadgePadX, y + (kBadgeHeight + kSmallFontCapHeight) / 2 - 1);
  gfx_.print(text);
  gfx_.setTextColor(black_);
  return x + width;
}

// ----------------------------------------------------------------------------------------------------------------
// Graph
// ----------------------------------------------------------------------------------------------------------------

void DashboardPainter::drawGraph(const TemperatureHistory &history, const GraphWindow &window) {
  gfx_.setFont(nullptr);
  gfx_.setTextColor(black_);

  // Grid lines and temperature labels.
  for (int temp = kMinTemp; temp <= kMaxTemp; temp += kGridStep) {
    const int16_t y = yForTemperature(temp, kGraphY, kGraphHeight, kMinTemp, kMaxTemp);
    if (temp != kMinTemp && temp != kMaxTemp) {
      for (int16_t x = kGraphX + 2; x < kGraphX + kGraphWidth - 2; x += 4) {
        gfx_.drawPixel(x, y, black_);
      }
    }
    char label[6];
    snprintf(label, sizeof(label), "%d", temp);
    drawTextRightAligned(kGraphX - 5, y - kSmallFontHeight / 2, label);
  }

  // Time ticks and labels in minutes.
  const uint32_t firstTick = ((window.startS + window.tickS - 1) / window.tickS) * window.tickS;
  for (uint32_t t = firstTick; t <= window.startS + window.spanS; t += window.tickS) {
    const int16_t x = window.xFor(t, kGraphX, kGraphWidth);
    gfx_.drawFastVLine(x, graphBottom() + 1, 3, black_);
    char label[12];
    snprintf(label, sizeof(label), "%lu", static_cast<unsigned long>(t / 60));
    const int16_t width = textWidth(label);
    int16_t xLabel = x - width / 2;
    if (xLabel + width > kPanelWidth - kGap) {
      xLabel = kPanelWidth - kGap - width;
    }
    gfx_.setCursor(xLabel, kAxisLabelTop);
    gfx_.print(label);
  }
  gfx_.setCursor(2, kAxisLabelTop);
  gfx_.print("min");

  gfx_.drawRect(kGraphX, kGraphY, kGraphWidth, kGraphHeight, black_);
  drawTraces(history, window);
  drawShotMarkers(history, window);
  drawLegend();
}

void DashboardPainter::drawTraces(const TemperatureHistory &history, const GraphWindow &window) {
  const int16_t top = kGraphY + 1;
  const int16_t bottom = graphBottom() - 1;
  const int16_t height = bottom - top + 1;
  bool havePrevious = false;
  int16_t xPrev = 0;
  int16_t yHxPrev = 0;
  int16_t xLastSteamDot = -1;

  for (uint16_t i = 0; i < history.size(); ++i) {
    const uint32_t t = history.timeAt(i);
    if (t < window.startS) {
      continue;
    }
    const TemperatureHistory::Sample sample = history.at(i);
    const int16_t x = window.xFor(t, kGraphX + 1, kGraphWidth - 2);
    const int16_t yHx = yForTemperature(sample.hx, top, height, kMinTemp, kMaxTemp);
    const int16_t ySteam = yForTemperature(sample.steam, top, height, kMinTemp, kMaxTemp);

    // Heat exchanger: solid, two pixels thick.
    if (havePrevious) {
      gfx_.drawLine(xPrev, yHxPrev, x, yHx, black_);
      gfx_.drawLine(xPrev, yHxPrev - 1, x, yHx - 1, black_);
    } else {
      gfx_.drawPixel(x, yHx, black_);
      gfx_.drawPixel(x, yHx - 1, black_);
    }
    // Steam boiler: a dot on every second pixel column, so it stays dotted no matter how wide the window is.
    if (x % 2 == 0 && x != xLastSteamDot) {
      gfx_.drawPixel(x, ySteam, black_);
      xLastSteamDot = x;
    }

    havePrevious = true;
    xPrev = x;
    yHxPrev = yHx;
  }
}

void DashboardPainter::drawShotMarkers(const TemperatureHistory &history, const GraphWindow &window) {
  gfx_.setFont(nullptr);
  const uint32_t endS = window.startS + window.spanS;
  for (uint8_t i = 0; i < history.shotCount(); ++i) {
    const TemperatureHistory::Shot shot = history.shot(i);
    if (shot.startS < window.startS || shot.startS > endS) {
      continue;
    }
    const int16_t x = window.xFor(shot.startS, kGraphX + 1, kGraphWidth - 2);
    // Triangle hanging from the top edge, a dotted drop line and the shot length next to it.
    gfx_.fillTriangle(x - kMarkerHalfWidth, kGraphY + 1, x + kMarkerHalfWidth, kGraphY + 1, x,
                      kGraphY + 1 + kMarkerHeight, black_);
    for (int16_t y = kGraphY + kMarkerHeight + 3; y < graphBottom(); y += 6) {
      gfx_.drawPixel(x, y, black_);
    }
    char label[8];
    snprintf(label, sizeof(label), "%us", static_cast<unsigned>(shot.durationS));
    // Two shots shortly after each other would print on top of each other, then only the later one gets a label.
    const bool nextIsClose = i + 1 < history.shotCount() && history.shot(i + 1).startS <= endS &&
                             window.xFor(history.shot(i + 1).startS, kGraphX + 1, kGraphWidth - 2) - x <
                                 textWidth(label) + kMarkerHalfWidth + 8;
    if (!nextIsClose) {
      gfx_.setCursor(x + kMarkerHalfWidth + 2, kGraphY + 3);
      gfx_.print(label);
    }
  }
}

void DashboardPainter::drawLegend() {
  // Bottom right corner inside the graph, on a white box, so the traces do not run through the text.
  gfx_.setFont(nullptr);
  gfx_.setTextColor(black_);
  const int16_t lineWidth = 14;
  const int16_t hxWidth = textWidth("HX");
  const int16_t steamWidth = textWidth("STEAM");
  const int16_t width = 4 + lineWidth + 4 + hxWidth + 8 + lineWidth + 4 + steamWidth + 4;
  const int16_t height = kSmallFontHeight + 4;
  const int16_t x = kGraphX + kGraphWidth - width - 2;
  const int16_t y = graphBottom() - height - 1;
  gfx_.fillRect(x, y, width, height, white_);
  int16_t cursor = x + 4;
  gfx_.fillRect(cursor, y + height / 2 - 1, lineWidth, 2, black_);
  cursor += lineWidth + 4;
  gfx_.setCursor(cursor, y + 2);
  gfx_.print("HX");
  cursor += hxWidth + 8;
  for (int16_t px = cursor; px < cursor + lineWidth; px += 2) {
    gfx_.drawPixel(px, y + height / 2, black_);
  }
  cursor += lineWidth + 4;
  gfx_.setCursor(cursor, y + 2);
  gfx_.print("STEAM");
}

void DashboardPainter::drawWaitingNote(const DisplayModel &model) {
  // Nothing to plot yet: say so in the empty graph.
  gfx_.setFont(&FreeSans9pt7b);
  gfx_.setTextColor(black_);
  const int16_t xCenter = kGraphX + kGraphWidth / 2;
  const int16_t yCenter = kGraphY + kGraphHeight / 2;
  drawTextCentered(xCenter, yCenter - 4, "Waiting for the Mara X ...");
  if (model.wifi == DisplayModel::Wifi::Portal && model.apName) {
    char text[64];
    snprintf(text, sizeof(text), "Wifi setup: connect to %s", model.apName);
    drawTextCentered(xCenter, yCenter + 20, text);
  }
}

// ----------------------------------------------------------------------------------------------------------------
// Text helpers
// ----------------------------------------------------------------------------------------------------------------

int16_t DashboardPainter::textWidth(const char *text) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  gfx_.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return static_cast<int16_t>(w);
}

void DashboardPainter::drawTextCentered(int16_t xCenter, int16_t baseline, const char *text) {
  gfx_.setCursor(xCenter - textWidth(text) / 2, baseline);
  gfx_.print(text);
}

void DashboardPainter::drawTextRightAligned(int16_t xRight, int16_t baseline, const char *text) {
  gfx_.setCursor(xRight - textWidth(text), baseline);
  gfx_.print(text);
}
