// Known-answer oracle for the firmware port: boots the emulated Bittle, sends serial commands and checks what comes
// back on the UART and what the PCA9685 was told to do. Built with normal flags (no sanitizers) by build.sh.
#include "firmware.h"
#include <stdio.h>
#include <string.h>
#include <string>

namespace {

struct Reply {
  std::string out;
  uint16_t pwm[16];
};

Reply send(const char *cmd) {
  Reply r;
  fw::init_once();
  fw::run((const uint8_t *)cmd, strlen(cmd));
  r.out = mock::serial_out();
  for (int i = 0; i < 16; i++) r.pwm[i] = mock::pwm_off(i);
  return r;
}

int g_passed, g_failed;
bool g_dump;

std::string esc(const std::string &s) {
  std::string e;
  for (char c : s) {
    if (c == '\n') e += "\\n";
    else if (c == '\r') e += "\\r";
    else if (c == '\t') e += "\\t";
    else e += c;
  }
  return e;
}

struct Golden { const char *cmd; const char *out; uint16_t pwm[16]; };

// What the firmware is driven with. The expected replies live in oracle_golden.inc, recorded from the verified
// emulation (`oracle --dump`).
const char *const kCmds[] = {"?\n", "d\n", "kup\n", "j\n", "m8 45\n", "kwkF\n", "c\n", "ksit\n", "kvtF\n"};

#include "oracle_golden.inc"

void dump() {
  printf("const Golden kGolden[] = {\n");
  for (const char *c : kCmds) {
    Reply r = send(c);
    printf("  {\"%s\", \"%s\", {", esc(c).c_str(), esc(r.out).c_str());
    for (int i = 0; i < 16; i++) printf("%u%s", r.pwm[i], i < 15 ? ", " : "");
    printf("}},\n");
  }
  printf("};\n");
}

void check(const Golden &g) {
  Reply r = send(g.cmd);
  bool out_ok = r.out == g.out;
  bool pwm_ok = memcmp(r.pwm, g.pwm, sizeof r.pwm) == 0;
  if (out_ok && pwm_ok) { g_passed++; return; }
  g_failed++;
  printf("FAIL %s\n", esc(g.cmd).c_str());
  if (!out_ok) printf("  serial want: %s\n  serial got:  %s\n", esc(g.out).c_str(), esc(r.out).c_str());
  if (!pwm_ok) {
    printf("  pwm want:");
    for (int i = 0; i < 16; i++) printf(" %u", g.pwm[i]);
    printf("\n  pwm got: ");
    for (int i = 0; i < 16; i++) printf(" %u", r.pwm[i]);
    printf("\n");
  }
}

}  // namespace

int main(int argc, char **argv) {
  g_dump = argc > 1 && !strcmp(argv[1], "--dump");
  mock::capture_serial(true);

  if (g_dump) { dump(); return 0; }
  for (const Golden &g : kGolden) check(g);

  // Every command starts from the same booted robot: a walk in between must not change what "kup" does.
  Reply first = send("kup\n");
  send("kwkF\n");
  send("m8 45\n");
  Reply again = send("kup\n");
  if (first.out == again.out && memcmp(first.pwm, again.pwm, sizeof first.pwm) == 0 &&
      mock::trace_hash() != 0) g_passed++;
  else { g_failed++; printf("FAIL reset: kup is not repeatable after other commands\n"); }

  // A different command must be distinguishable from "kup" (the oracle is not comparing constants to themselves).
  Reply walk = send("kwkF\n");
  if (memcmp(first.pwm, walk.pwm, sizeof first.pwm) != 0) g_passed++;
  else { g_failed++; printf("FAIL kwkF and kup left the servos in the same pose\n"); }

  printf("ORACLE passed=%d failed=%d\n", g_passed, g_failed);
  return g_failed ? 1 : 0;
}
