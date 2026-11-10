// Fuzzes the OpenCat firmware's serial command interface: every byte a phone app, the Python UI or a terminal can
// send to a Petoi Bittle goes through read_serial() and the reaction() dispatcher exactly as on the robot. The bytes are
// delivered as they are: the firmware accepts a command with no line ending (io.h:44-46, the serial-monitor mode).
//
// One precondition: an input whose stream ends in a lone, unterminated command token is rejected with -1. read_serial()
// then evaluates newCmd[cmdLen - 1] with cmdLen == 0 (src/io.h:46), a read before the buffer that aborts every run under
// ASan/UBSan; it is a real firmware bug (known-findings/lone_token, with the reproducer), kept out of the fuzzed set so
// that Mayhem's startup probe can run. Every other input, including every other unterminated one, passes unmodified.
#include "firmware.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size > 60000) return -1;
  fw::init_once();
  return fw::run(data, size) ? 0 : -1;
}
