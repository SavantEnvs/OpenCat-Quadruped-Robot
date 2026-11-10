// Minimal host emulation of the Arduino core, enough to run Petoi's OpenCat firmware on x86-64.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <new>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#define ARDUINO_SHIM 1
#define PROGMEM
#define PSTR(s) (s)
#define PGM_P const char *
#define __PGMSPACE_H_ 1
#define BUFFER_LENGTH 32

typedef uint8_t byte;
typedef bool boolean;
typedef uint16_t word;
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(PSTR(s)))

// Reads one byte of "program memory". `a` is the address truncated the way the 16-bit AVR sees it.
uint8_t mock_pgm_read_byte(uintptr_t a);
#define pgm_read_byte(a) mock_pgm_read_byte((uintptr_t)(a))
#define pgm_read_word(a) ((uint16_t)(mock_pgm_read_byte((uintptr_t)(a)) | (mock_pgm_read_byte((uintptr_t)(a) + 1) << 8)))
#define pgm_read_float(a) (*(const float *)(a))
#define pgm_read_ptr(a) (*(void *const *)(a))

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LSBFIRST 0
#define MSBFIRST 1
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19
#define A6 20
#define A7 21

class String {
public:
  std::string s;
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(char c) : s(1, c) {}
  String(unsigned char v, unsigned char base = 10) { init((unsigned long)v, base, false); }
  String(int v, unsigned char base = 10) { init((long)v, base); }
  String(unsigned int v, unsigned char base = 10) { init((unsigned long)v, base, false); }
  String(long v, unsigned char base = 10) { init(v, base); }
  String(unsigned long v, unsigned char base = 10) { init(v, base, false); }
  String(float v, unsigned char d = 2) { fmt((double)v, d); }
  String(double v, unsigned char d = 2) { fmt(v, d); }
  String &operator+=(const String &o) { s += o.s; return *this; }
  String &operator+=(const char *c) { if (c) s += c; return *this; }
  String &operator+=(char c) { s += c; return *this; }
  String operator+(const String &o) const { String r(*this); r.s += o.s; return r; }
  String operator+(const char *c) const { String r(*this); if (c) r.s += c; return r; }
  const char *c_str() const { return s.c_str(); }
  unsigned int length() const { return (unsigned int)s.size(); }
private:
  void init(long v, unsigned char base) {
    if (base == 10) { s = std::to_string(v); return; }
    init((unsigned long)v, base, false);
  }
  void init(unsigned long v, unsigned char base, bool) {
    if (base == 10) { s = std::to_string(v); return; }
    char buf[80]; int n = 0;
    if (v == 0) buf[n++] = '0';
    while (v && n < 79) { int d = v % base; buf[n++] = d < 10 ? '0' + d : 'A' + d - 10; v /= base; }
    s.assign(buf, n); std::reverse(s.begin(), s.end());
  }
  void fmt(double v, unsigned char d) { char b[64]; snprintf(b, sizeof b, "%.*f", d, v); s = b; }
};

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  size_t write(const char *str) { size_t n = 0; while (str && *str) n += write((uint8_t)*str++); return n; }
  size_t write(const uint8_t *b, size_t n) { size_t k = 0; while (n--) k += write(*b++); return k; }
  size_t write(unsigned long n) { return write((uint8_t)n); }
  size_t write(long n) { return write((uint8_t)n); }
  size_t write(unsigned int n) { return write((uint8_t)n); }
  size_t write(int n) { return write((uint8_t)n); }
  size_t print(const __FlashStringHelper *f) { return write(reinterpret_cast<const char *>(f)); }
  size_t print(const String &s) { return write(s.c_str()); }
  size_t print(const char *c) { return write(c); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(unsigned char v, int base = DEC) { return print((unsigned long)v, base); }
  size_t print(int v, int base = DEC) { return print((long)v, base); }
  size_t print(unsigned int v, int base = DEC) { return print((unsigned long)v, base); }
  size_t print(long v, int base = DEC) {
    if (base == 0) return write((uint8_t)v);
    if (base == 10 && v < 0) return write('-') + print((unsigned long)(-(v + 1)) + 1, 10);
    return print((unsigned long)v, base);
  }
  size_t print(unsigned long v, int base = DEC) {
    if (base == 0) return write((uint8_t)v);
    return print(String(v, (unsigned char)base));
  }
  size_t print(double v, int d = 2) { return print(String(v, (unsigned char)d)); }
  size_t println() { return write('\r') + write('\n'); }
  template <class T> size_t println(T v) { return print(v) + println(); }
  template <class T, class U> size_t println(T v, U b) { return print(v, b) + println(); }
};

#include "HardwareSerial.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI 3.1415926535897932384626433832795
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

// The Arduino core's templates, which accept mixed argument types (std::min does not).
template <class A, class B> inline typename std::common_type<A, B>::type min(A a, B b) { return a < b ? a : b; }
template <class A, class B> inline typename std::common_type<A, B>::type max(A a, B b) { return a > b ? a : b; }
template <class T, class L, class H> inline T constrain(T x, L lo, H hi) { return x < lo ? (T)lo : (x > hi ? (T)hi : x); }
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
inline char *dtostrf(double v, signed char width, unsigned char prec, char *out) {
  snprintf(out, 64, "%*.*f", width, prec, v);
  return out;
}

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void pinMode(uint8_t, uint8_t);
void digitalWrite(uint8_t, uint8_t);
int digitalRead(uint8_t);
int analogRead(uint8_t);
void analogWrite(uint8_t, int);
void tone(uint8_t pin, unsigned int freq, unsigned long duration = 0);
void noTone(uint8_t pin);
void randomSeed(unsigned long seed);
long random(long max);
long random(long min, long max);
