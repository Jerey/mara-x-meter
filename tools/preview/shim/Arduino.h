// Minimal stand in for the Arduino core so Adafruit GFX and the dashboard painter compile on a desktop.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARDUINO
#define ARDUINO 100
#endif
#define PROGMEM
#define IRAM_ATTR
#define F(x) (x)
typedef bool boolean;
class __FlashStringHelper;

#define radians(deg) ((deg) * M_PI / 180.0)
#define degrees(rad) ((rad) * 180.0 / M_PI)
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

inline void yield() {}
inline void optimistic_yield(uint32_t) {}

class String {
 public:
  String(const char *s = "") : text_(s ? s : "") {}
  size_t length() const { return strlen(text_); }
  const char *c_str() const { return text_; }

 private:
  const char *text_;
};

#include "Print.h"
