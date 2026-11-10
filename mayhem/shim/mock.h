// Host-side model of the NyBoard peripherals (UART, clock, EEPROM, PCA9685, 24C64 I2C EEPROM).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>

namespace mock {
constexpr size_t kEepromSize = 1024;       // ATmega328P on-board EEPROM
constexpr size_t kI2cEepromSize = 8192;    // 24C64 on the NyBoard

// Wipes the clock, UART, PCA9685 and traces. EEPROM contents are left alone.
void reset_peripherals();

void rx_set(const uint8_t *data, size_t n);
size_t rx_pending();
// While true, Serial.available() never fabricates incoming bytes (the firmware is waiting for a command's tail).
extern bool reading_cmd;

uint8_t *eeprom();
uint8_t *i2c_eeprom();
uint8_t *pca_regs();   // PCA9685 register file (256 bytes); survives reset_peripherals() only if the caller restores it

void capture_serial(bool on);
const std::string &serial_out();
void clear_serial_out();

uint16_t pwm_off(int channel);       // last 12-bit OFF count written to a PCA9685 channel
uint64_t pwm_writes();               // number of channel updates
uint64_t trace_hash();               // hash over every PCA9685 channel update and tone, in order
unsigned tone_count();
unsigned tone_last();
uint64_t now_us();
void set_now_us(uint64_t us);
}  // namespace mock
