// The OpenCat firmware (OpenCat.ino's default Bittle / NyBoard V1.2 configuration, minus the IMU and IR modules
// that need real hardware) compiled for the host, plus the machinery to boot it and run it on a byte stream.
#pragma once
#define MAIN_SKETCH
#define BITTLE
#define NyBoard_V1_2
#define OTHER_MODULES
#include "src/OpenCat.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "factory_image.inc"
#include "mock.h"

// Only the instincts' I2C EEPROM holds skill data on this board; "zero" is the one skill kept in program memory,
// and its address reaches pgm_read_byte() truncated to the AVR's 16 bits.
uint8_t mock_pgm_read_byte(uintptr_t a) {
  uintptr_t off = (uint16_t)(a - (uint16_t)(uintptr_t)zero);
  return off < sizeof(zero) ? (uint8_t)zero[off] : 0xFF;
}

namespace fw {

// Every firmware global that can change after boot. Booted state is captured once and restored before each run,
// so a run's result depends only on its input.
struct Snapshot {
  int8_t servoCalib_[DOF];
  int zeroPosition_[DOF], calibratedZeroPosition_[DOF], currentAng_[DOF];
  float currentAdjust_[DOF];
  long idleTimer_;
  int randomInterval_, uptime_, frame_;
  byte tStep_;
  char token_, lowerToken_, lastToken_;
  byte newCmdIdx_;
  int cmdLen_;
  int8_t periodGlobal_;
  char newCmd_[BUFF_LEN + 1], lastCmd_[CMD_LEN + 1];
  int spaceAfterStoringData_;
  int8_t yprTilt_[3];
  int lastVoltage_;
  bool servoOff_, fineAdjust_, gyroBalanceQ_, printGyro_, walkingQ_, manualHeadQ_, nonHeadJointQ_, hardServoQ_,
    manualEyeColorQ_;
  int targetHead_[HEAD_GROUP_LEN];
  int exceptions_;
  byte transformSpeed_;
  float protectiveShift_;
  long loopTimer_;
  byte fps_;
  float postureOrWalkingFactor_, RollPitchDeviation_[2];
  int8_t slope_;
  bool calibrated_;
  int lastValue_;
  long initValue_;
  byte match_;
  bool EEPROMOverflow_;
  long taskTimer_, taskInterval_;
  Skill skill_;
  Petoi_PWMServoDriver pwm_;
  uint8_t eeprom_[mock::kEepromSize], i2c_[mock::kI2cEepromSize], pca_[256];
  uint64_t clock_us_;
};

static Snapshot *g_boot;

#define FW_FIELDS(X) \
  X(servoCalib) X(zeroPosition) X(calibratedZeroPosition) X(currentAng) X(currentAdjust) X(idleTimer) \
  X(randomInterval) X(uptime) X(frame) X(tStep) X(token) X(lowerToken) X(lastToken) X(newCmdIdx) X(cmdLen) \
  X(periodGlobal) X(newCmd) X(lastCmd) X(spaceAfterStoringData) X(yprTilt) X(lastVoltage) X(servoOff) \
  X(fineAdjust) X(gyroBalanceQ) X(printGyro) X(walkingQ) X(manualHeadQ) X(nonHeadJointQ) X(hardServoQ) \
  X(manualEyeColorQ) X(targetHead) X(exceptions) X(transformSpeed) X(protectiveShift) X(loopTimer) X(fps) \
  X(postureOrWalkingFactor) X(RollPitchDeviation) X(slope) X(calibrated) X(lastValue) X(initValue) X(match) \
  X(EEPROMOverflow) X(taskTimer) X(taskInterval) X(skill) X(pwm)

// Skill::assignSkillAddressToOnboardEeprom() verbatim except for the loop bound: upstream computes the number of
// program-memory skills as sizeof(progmemPointer) / 2, which assumes the AVR's 2-byte pointers (one skill, "zero",
// here) and is 4x too large on a 64-bit host, where the loop would then run off the end of the skill table.
inline void assign_skill_addresses() {
  int skillAddressShift = 0;
  const int nProgmemSkills = sizeof(progmemPointer) / sizeof(progmemPointer[0]);
  for (byte s = 0; s < nProgmemSkills; s++) {
    byte nameLen = EEPROM.read(SKILLS + skillAddressShift++);
    skillAddressShift += nameLen;
    char skillType = EEPROM.read(SKILLS + skillAddressShift++);
    if (skillType == 'N')
      EEPROMWriteInt(SKILLS + skillAddressShift + 1, (int)(intptr_t)progmemPointer[s]);
    else
      s--;
    skillAddressShift += 3;
  }
}

// initRobot() for this configuration (no IMU, no IR receiver), calling assign_skill_addresses() as above.
inline void init_robot() {
  PTL('k');
  PTLF("\n* Start *");
  PTLF(MODEL);
  PTLF(SOFTWARE_VERSION);
  if (eeprom(BOOTUP_SOUND_STATE)) playMelody(MELODY_NORMAL);
  servoSetup();
  assign_skill_addresses();
  tQueue = new TaskQueue();
  int rndSeed = analogRead(A2);
  randomSeed(rndSeed);
  allCalibratedPWM(currentAng);
  delay(500);
  lastCmd[0] = '\0';
  token = T_REST;  // initRobot's GYRO_PIN branch; the first loop() pass loads the rest posture
  newCmdIdx = 2;
  PTLF("Ready!");
  beep(20);
}

inline void boot_robot() {
  mock::reset_peripherals();
  memcpy(mock::eeprom(), kFactoryEeprom, sizeof kFactoryEeprom);
  memcpy(mock::i2c_eeprom(), kFactoryI2cEeprom, sizeof kFactoryI2cEeprom);
  randomSeed(1);
  Serial.begin(BAUD_RATE);
  Wire.begin();
  init_robot();
  reaction();
}

inline void save_boot_state() {
  g_boot = new Snapshot;
#define X(n) memcpy(&g_boot->n##_, &n, sizeof n);
  FW_FIELDS(X)
#undef X
  memcpy(g_boot->eeprom_, mock::eeprom(), sizeof g_boot->eeprom_);
  memcpy(g_boot->i2c_, mock::i2c_eeprom(), sizeof g_boot->i2c_);
  memcpy(g_boot->pca_, mock::pca_regs(), sizeof g_boot->pca_);
  g_boot->clock_us_ = mock::now_us();
}

inline void restore_boot_state() {
#define X(n) memcpy(&n, &g_boot->n##_, sizeof n);
  FW_FIELDS(X)
#undef X
  mock::reset_peripherals();
  memcpy(mock::eeprom(), g_boot->eeprom_, sizeof g_boot->eeprom_);
  memcpy(mock::i2c_eeprom(), g_boot->i2c_, sizeof g_boot->i2c_);
  memcpy(mock::pca_regs(), g_boot->pca_, sizeof g_boot->pca_);
  mock::set_now_us(g_boot->clock_us_);
  randomSeed(1);
}

inline void init_once() {
  if (g_boot) return;
  boot_robot();
  save_boot_state();
}

// One pass of OpenCat.ino's loop() for this configuration. Returns false, before running the pass, when it would be
// read_serial() taking the last pending byte as a command token with nothing before it in the command buffer: that is
// src/io.h:46 evaluating newCmd[cmdLen - 1] with cmdLen == 0 (known-findings/lone_token). It is the only input class
// skipped, and only for that line: the stream ends while a lone token is unterminated.
inline bool firmware_loop() {
  if (!tQueue->empty()) {
    tQueue->popTask();
  } else {
    if (mock::rx_pending() == 1 && cmdLen == 0) return false;
    mock::reading_cmd = true;
    readSignal();
    mock::reading_cmd = false;
  }
  reaction();
  lowBattery();
  return true;
}

// Delivers `data` on the serial port and runs the firmware until it has consumed it and gone quiet. Returns false
// when the stream ends in the lone token that firmware_loop() declines to read.
inline bool run(const uint8_t *data, size_t size) {
  restore_boot_state();
  mock::rx_set(data, size);
  unsigned quiet = 0;
  for (unsigned i = 0; i < 4096 && quiet < 96; i++) {
    if (!firmware_loop()) return false;
    quiet = mock::rx_pending() ? 0 : quiet + 1;
  }
  return true;
}

}  // namespace fw
