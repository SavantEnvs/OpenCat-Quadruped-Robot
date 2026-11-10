#pragma once
#include <stdint.h>

class EEPROMClass {
public:
  uint8_t read(int idx);
  void write(int idx, uint8_t v);
  void update(int idx, uint8_t v);
  int length() const { return 1024; }
};

extern EEPROMClass EEPROM;
