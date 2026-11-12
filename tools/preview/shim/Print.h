#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    while (size--) {
      n += write(*buffer++);
    }
    return n;
  }
  size_t write(const char *s) { return write(reinterpret_cast<const uint8_t *>(s), strlen(s)); }
  size_t print(const char *s) { return write(s); }
  size_t print(char c) { return write(static_cast<uint8_t>(c)); }
  size_t print(int v) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", v);
    return write(buffer);
  }
  size_t print(unsigned int v) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%u", v);
    return write(buffer);
  }
  size_t print(long v) {
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%ld", v);
    return write(buffer);
  }
  size_t print(unsigned long v) {
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%lu", v);
    return write(buffer);
  }
  size_t println(const char *s = "") { return print(s) + write('\n'); }
};
