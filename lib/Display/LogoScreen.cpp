#include "LogoScreen.hpp"

#include <Fonts/FreeSans9pt7b.h>

#include "DisplayModel.hpp"
#include "LogoImage.h"

namespace {

/// @brief Small deterministic random generator, so the picture only depends on the seed.
class Dice {
 public:
  explicit Dice(uint32_t seed) : state_(seed * 2654435761u + 0x9E3779B9u) {}

  /// @brief Uniform value in [0, n).
  uint32_t roll(uint32_t n) {
    state_ = state_ * 1664525u + 1013904223u;
    return n ? (state_ >> 8) % n : 0;
  }

  /// @brief Uniform value in [low, high].
  int16_t between(int16_t low, int16_t high) { return static_cast<int16_t>(low + roll(high - low + 1)); }

 private:
  uint32_t state_;
};

constexpr int16_t kRowBytes = kLogoWidth / 8;
constexpr int16_t kMargin = 8;
constexpr int16_t kTagHeight = 22;
constexpr int16_t kMaxShift = 14;
constexpr int16_t kWordMarkRows = 116;  // the rows of the picture with the word mark, the plate below stays intact

/// @brief Draws the rows [firstRow, firstRow + rows) of the logo at the given position, optionally inverted.
void drawBand(Adafruit_GFX &gfx, int16_t x, int16_t y, int16_t firstRow, int16_t rows, bool inverted, uint16_t black,
              uint16_t white) {
  const unsigned char *rowsStart = kLogo + static_cast<int32_t>(firstRow) * kRowBytes;
  // The picture stores 1 = white, so set bits are drawn in white unless the band is inverted.
  gfx.drawBitmap(x, y + firstRow, rowsStart, kLogoWidth, rows, inverted ? black : white, inverted ? white : black);
}

void drawCornerBrackets(Adafruit_GFX &gfx, Dice &dice, uint16_t black) {
  const int16_t inset = dice.between(4, 14);
  const int16_t arm = dice.between(12, 28);
  const int16_t x0 = inset;
  const int16_t x1 = kPanelWidth - 1 - inset;
  const int16_t y0 = inset;
  const int16_t y1 = kPanelHeight - 1 - inset;
  gfx.fillRect(x0, y0, arm, 2, black);
  gfx.fillRect(x0, y0, 2, arm, black);
  gfx.fillRect(x1 - arm + 1, y0, arm, 2, black);
  gfx.fillRect(x1 - 1, y0, 2, arm, black);
  gfx.fillRect(x0, y1 - 1, arm, 2, black);
  gfx.fillRect(x0, y1 - arm + 1, 2, arm, black);
  gfx.fillRect(x1 - arm + 1, y1 - 1, arm, 2, black);
  gfx.fillRect(x1 - 1, y1 - arm + 1, 2, arm, black);
}

/// @brief Thin "scan lines" and small blocks above and below the logo, different every time.
void drawNoise(Adafruit_GFX &gfx, Dice &dice, int16_t logoY, uint16_t black) {
  const int16_t aboveEnd = logoY - 4;
  const int16_t belowStart = logoY + kLogoHeight + 4;
  const int16_t belowEnd = kPanelHeight - kTagHeight - 4;  // keeps the tag line clean

  const int lines = dice.between(2, 4);
  for (int i = 0; i < lines; ++i) {
    const bool above = dice.roll(2) == 0 && aboveEnd > 8;
    const int16_t y = above ? dice.between(6, aboveEnd) : dice.between(belowStart, belowEnd);
    const int16_t x = dice.between(0, kPanelWidth / 2);
    const int16_t length = dice.between(40, kPanelWidth - x);
    gfx.drawFastHLine(x, y, length, black);
    if (dice.roll(2) == 0) {
      gfx.fillRect(x + length - 6, y - 2, 6, 5, black);
    }
  }
  const int blocks = dice.between(3, 7);
  for (int i = 0; i < blocks; ++i) {
    const int16_t size = dice.between(3, 7);
    const int16_t x = dice.between(0, kPanelWidth - size);
    const bool above = dice.roll(2) == 0 && aboveEnd > size;
    const int16_t y = above ? dice.between(0, aboveEnd - size) : dice.between(belowStart, belowEnd - size);
    gfx.fillRect(x, y, size, size, black);
  }
}

void drawTag(Adafruit_GFX &gfx, Dice &dice, const char *tag, uint16_t black) {
  if (!tag) {
    return;
  }
  gfx.setFont(&FreeSans9pt7b);
  gfx.setTextColor(black);
  gfx.setTextSize(1);
  int16_t bx, by;
  uint16_t bw, bh;
  gfx.getTextBounds(tag, 0, 0, &bx, &by, &bw, &bh);
  const int16_t x = dice.between(kMargin + 10, kPanelWidth - kMargin - 10 - static_cast<int16_t>(bw));
  const int16_t baseline = kPanelHeight - dice.between(10, 16);
  gfx.setCursor(x, baseline);
  gfx.print(tag);
  // A cursor block behind the text.
  gfx.fillRect(x + bw + 4, baseline - 10, 7, 11, black);
}

}  // namespace

void LogoScreen::paint(Adafruit_GFX &gfx, uint16_t black, uint16_t white, uint32_t seed, const char *tag) {
  Dice dice(seed);

  const int16_t x = dice.between(kMargin + kMaxShift, kPanelWidth - kLogoWidth - kMargin - kMaxShift);
  const int16_t y = dice.between(kMargin, kPanelHeight - kLogoHeight - kTagHeight - kMargin);

  // Cut the word mark into bands. Every second band is shifted a little, one band may be inverted.
  const int bands = dice.between(3, 6);
  const int inverted = dice.roll(3) == 0 ? dice.between(0, bands - 1) : -1;
  int16_t row = 0;
  for (int band = 0; band < bands; ++band) {
    const int16_t remaining = kWordMarkRows - row;
    const int16_t rows = band == bands - 1
                             ? remaining
                             : dice.between(remaining / (bands - band) / 2, remaining - (bands - band - 1) * 6);
    const int16_t shift = band % 2 == 0 ? 0 : dice.between(-kMaxShift, kMaxShift);
    drawBand(gfx, x + shift, y, row, rows, band == inverted, black, white);
    row += rows;
  }
  // The plate with "METER" and the tag line stay readable.
  drawBand(gfx, x, y, kWordMarkRows, kLogoHeight - kWordMarkRows, false, black, white);

  drawCornerBrackets(gfx, dice, black);
  drawNoise(gfx, dice, y, black);
  drawTag(gfx, dice, tag, black);
}
