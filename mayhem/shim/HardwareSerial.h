#pragma once
#include <stddef.h>
#include <stdint.h>

class HardwareSerial : public Print {
public:
  void begin(unsigned long) {}
  void end() {}
  void setTimeout(unsigned long) {}
  void flush() {}
  int available();
  int read();
  int peek();
  size_t write(uint8_t c) override;
  using Print::write;
};

extern HardwareSerial Serial;
