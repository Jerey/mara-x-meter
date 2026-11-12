#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

/**
 * @brief Paints the logo as a full screen picture, shown while the meter starts and after it is switched off.
 *
 * The off screen stays on the panel for hours or days, so the picture must not be the same every time: the logo
 * sits at a random position, the word mark is cut into bands, which are shifted (and sometimes inverted), and some
 * random decorations are scattered around. Everything is derived from the seed, so the same seed paints the same
 * picture, which the host side preview makes use of.
 */
class LogoScreen {
 public:
  /// @brief Paints onto a canvas cleared to white. @param tag Short caption in small letters, may be nullptr.
  static void paint(Adafruit_GFX &gfx, uint16_t black, uint16_t white, uint32_t seed, const char *tag);
};
