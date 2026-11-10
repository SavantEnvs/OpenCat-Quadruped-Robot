#pragma once
#include <stddef.h>
#include <stdint.h>

class TwoWire {
public:
  void begin() {}
  void setClock(uint32_t) {}
  void beginTransmission(uint8_t addr);
  uint8_t endTransmission(bool stop = true);
  size_t write(uint8_t v);
  size_t write(int v) { return write((uint8_t)v); }
  size_t write(const uint8_t *b, size_t n) { size_t k = 0; while (n--) k += write(*b++); return k; }
  uint8_t requestFrom(uint8_t addr, uint8_t qty, uint8_t stop = 1);
  uint8_t requestFrom(int addr, int qty) { return requestFrom((uint8_t)addr, (uint8_t)qty, (uint8_t)1); }
  int available();
  int read();
  int peek();
};

extern TwoWire Wire;

// AVR's int is 16 bits, so upstream's EEPROMReadInt() (src/eeprom.h) returns a signed value there (angle limits
// such as -120 read back as -120). On this host int is 32 bits and it would return 0..65535. OpenCat.h first
// includes <Wire.h> (via PCA9685servo.h) after eeprom.h and before every caller, so this is the one place a
// wrapper can be slotted in without touching upstream. The inner name is not re-expanded. loadAngleLimit() is
// defined in eeprom.h on top of EEPROMReadInt(), so it needs its own wrapper.
#ifdef EEPROMReadInt
#error "EEPROMReadInt hook installed twice"
#endif
#define EEPROMReadInt(addr) ((int)(int16_t)EEPROMReadInt(addr))
#define loadAngleLimit(idx, dim) ((int)(int16_t)loadAngleLimit(idx, dim))
