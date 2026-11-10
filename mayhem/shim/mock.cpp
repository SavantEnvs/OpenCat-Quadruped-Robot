#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
#include "mock.h"

#include <string.h>

namespace {
uint64_t g_us;
uint8_t g_rx[1 << 16];
size_t g_rx_head, g_rx_tail;
unsigned g_idle_polls;
std::string g_out;
bool g_capture;

uint8_t g_eeprom[mock::kEepromSize];
uint8_t g_i2c_eeprom[mock::kI2cEepromSize];

uint8_t g_pca[256];
uint16_t g_pwm_off[16];
uint64_t g_pwm_writes;
uint64_t g_hash = 1469598103934665603ULL;
unsigned g_tone_count, g_tone_last;
uint64_t g_pins;

void mix(uint64_t v) {
  g_hash ^= v;
  g_hash *= 1099511628211ULL;
}

// TwoWire transaction state
uint8_t g_addr;
uint8_t g_txbuf[64];
size_t g_txlen;
uint8_t g_rxbuf[64];
size_t g_rxlen, g_rxpos;
uint8_t g_pca_ptr;
unsigned g_ee_ptr;

void pca_write(uint8_t reg, uint8_t v) {
  g_pca[reg] = v;
  if (reg >= 0x06 && reg < 0x06 + 64 && (reg - 0x06) % 4 == 3) {  // LEDn_OFF_H completes a channel update
    int ch = (reg - 0x06) / 4;
    uint16_t off = (uint16_t)(g_pca[reg - 1] | ((g_pca[reg] & 0x1F) << 8));
    g_pwm_off[ch] = off;
    g_pwm_writes++;
    mix(((uint64_t)ch << 16) | off);
  }
}
}  // namespace

namespace mock {
bool reading_cmd;

void reset_peripherals() {
  g_us = 0;
  g_rx_head = g_rx_tail = 0;
  g_idle_polls = 0;
  g_out.clear();
  memset(g_pca, 0, sizeof g_pca);
  memset(g_pwm_off, 0, sizeof g_pwm_off);
  g_pwm_writes = 0;
  g_hash = 1469598103934665603ULL;
  g_tone_count = g_tone_last = 0;
  g_pins = 0;
  g_txlen = g_rxlen = g_rxpos = 0;
  g_pca_ptr = 0;
  g_ee_ptr = 0;
  reading_cmd = false;
}
void rx_set(const uint8_t *d, size_t n) {
  if (n > sizeof g_rx) n = sizeof g_rx;
  memcpy(g_rx, d, n);
  g_rx_head = 0;
  g_rx_tail = n;
}
size_t rx_pending() { return g_rx_tail - g_rx_head; }
uint8_t *eeprom() { return g_eeprom; }
uint8_t *i2c_eeprom() { return g_i2c_eeprom; }
uint8_t *pca_regs() { return g_pca; }
void capture_serial(bool on) { g_capture = on; }
const std::string &serial_out() { return g_out; }
void clear_serial_out() { g_out.clear(); }
uint16_t pwm_off(int ch) { return g_pwm_off[ch & 15]; }
uint64_t pwm_writes() { return g_pwm_writes; }
uint64_t trace_hash() { return g_hash; }
unsigned tone_count() { return g_tone_count; }
unsigned tone_last() { return g_tone_last; }
uint64_t now_us() { return g_us; }
void set_now_us(uint64_t us) { g_us = us; }
}  // namespace mock

HardwareSerial Serial;
TwoWire Wire;
EEPROMClass EEPROM;

int HardwareSerial::available() {
  if (g_rx_head < g_rx_tail) return (int)(g_rx_tail - g_rx_head);
  g_us += 1000;  // an empty poll costs a millisecond of firmware time, so every wait-loop makes progress
  if (!mock::reading_cmd && ++g_idle_polls > 3) {
    // The robot is polling for an interrupting command (running a behavior): one arrives.
    g_idle_polls = 0;
    if (g_rx_tail >= sizeof g_rx) return 0;
    g_rx[g_rx_tail++] = '\n';
    return 1;
  }
  return 0;
}
int HardwareSerial::read() {
  if (g_rx_head >= g_rx_tail) return -1;
  g_idle_polls = 0;
  return g_rx[g_rx_head++];
}
int HardwareSerial::peek() { return g_rx_head < g_rx_tail ? g_rx[g_rx_head] : -1; }
size_t HardwareSerial::write(uint8_t c) {
  if (g_capture && g_out.size() < (1u << 20)) g_out.push_back((char)c);
  return 1;
}

uint8_t EEPROMClass::read(int idx) { return g_eeprom[(unsigned)idx & (mock::kEepromSize - 1)]; }
void EEPROMClass::write(int idx, uint8_t v) { g_eeprom[(unsigned)idx & (mock::kEepromSize - 1)] = v; }
void EEPROMClass::update(int idx, uint8_t v) { write(idx, v); }

void TwoWire::beginTransmission(uint8_t addr) {
  g_addr = addr;
  g_txlen = 0;
}
size_t TwoWire::write(uint8_t v) {
  if (g_txlen < sizeof g_txbuf) g_txbuf[g_txlen++] = v;
  return 1;
}
uint8_t TwoWire::endTransmission(bool) {
  if (g_addr == 0x40) {  // PCA9685, register auto-increment
    if (g_txlen >= 1) g_pca_ptr = g_txbuf[0];
    for (size_t i = 1; i < g_txlen; i++) pca_write(g_pca_ptr++, g_txbuf[i]);
    return 0;
  }
  if (g_addr == 0x54) {  // 24C64: 16-bit address, then page writes that wrap inside a 32-byte page
    if (g_txlen >= 2) g_ee_ptr = (((unsigned)g_txbuf[0] << 8) | g_txbuf[1]) & (mock::kI2cEepromSize - 1);
    for (size_t i = 2; i < g_txlen; i++) {
      g_i2c_eeprom[g_ee_ptr] = g_txbuf[i];
      g_ee_ptr = (g_ee_ptr & ~31u) | ((g_ee_ptr + 1) & 31u);
    }
    return 0;
  }
  return 2;  // NACK on address
}
uint8_t TwoWire::requestFrom(uint8_t addr, uint8_t qty, uint8_t) {
  g_rxlen = g_rxpos = 0;
  if (qty > sizeof g_rxbuf) qty = sizeof g_rxbuf;
  for (uint8_t i = 0; i < qty; i++) {
    if (addr == 0x40) g_rxbuf[g_rxlen++] = g_pca[g_pca_ptr++];
    else if (addr == 0x54) {
      g_rxbuf[g_rxlen++] = g_i2c_eeprom[g_ee_ptr];
      g_ee_ptr = (g_ee_ptr + 1) & (mock::kI2cEepromSize - 1);
    } else break;
  }
  return (uint8_t)g_rxlen;
}
int TwoWire::available() { return (int)(g_rxlen - g_rxpos); }
int TwoWire::read() { return g_rxpos < g_rxlen ? g_rxbuf[g_rxpos++] : -1; }
int TwoWire::peek() { return g_rxpos < g_rxlen ? g_rxbuf[g_rxpos] : -1; }

unsigned long millis() { return (unsigned long)((g_us / 1000) & 0xFFFFFFFFu); }
unsigned long micros() { return (unsigned long)(g_us & 0xFFFFFFFFu); }
void delay(unsigned long ms) { g_us += (uint64_t)ms * 1000; }
void delayMicroseconds(unsigned int us) { g_us += us; }
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t pin, uint8_t v) {
  uint64_t bit = 1ULL << (pin & 63);
  g_pins = v ? (g_pins | bit) : (g_pins & ~bit);
}
int digitalRead(uint8_t pin) { return (int)((g_pins >> (pin & 63)) & 1); }
int analogRead(uint8_t) { return 700; }  // battery above LOW_VOLTAGE, nothing on the grove sockets
void analogWrite(uint8_t, int) {}
void tone(uint8_t, unsigned int freq, unsigned long) {
  g_tone_count++;
  g_tone_last = freq;
  mix(0x100000000ULL | freq);
}
void noTone(uint8_t) {}
void randomSeed(unsigned long seed) { srandom((unsigned)seed); }
long random(long max) { return max <= 0 ? 0 : (long)(::random() % max); }
long random(long min, long max) { return min >= max ? min : min + random(max - min); }
