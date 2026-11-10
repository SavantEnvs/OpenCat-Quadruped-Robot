# Unterminated lone command token reads `newCmd[-1]`

**Where:** `src/io.h:46` (`read_serial()`), `newCmd[cmdLen - 1]` evaluated with `cmdLen == 0`.

**Cause:** after reading the command token, `read_serial()` enters a `do { ... } while (newCmd[cmdLen - 1] != terminator && ...)` loop.
When nothing else has arrived yet (a single token with no terminator, e.g. the byte `d` sent from a serial monitor set to
"no line ending", which the README recommends), the first `while` test indexes `newCmd[-1]`. ASan reports a
global-buffer-overflow read and UBSan an out-of-bounds index.

**Impact:** a one-byte read before `newCmd`. Benign on the AVR board (it reads a neighbouring global), but it is undefined behaviour
and it is reachable from any one-byte serial input.

**One-line fix:** test the length first, `while (cmdLen == 0 || (newCmd[cmdLen - 1] != terminator && ...))`.

**How the fuzz target treats it:** the target delivers raw bytes with no framing, and any input whose stream ends while a lone token is
unterminated (for example the byte `d` sent alone) reaches this line. Left in, every such input aborts the run, and Mayhem's startup probe
(`-runs=5` over the stored corpus) fails before any coverage is reported. So `fw::firmware_loop()` (`mayhem/harness/firmware.h`) declines to read
exactly that case, a pass that would have `read_serial()` take the last pending byte as the token with `cmdLen == 0`, and `LLVMFuzzerTestOneInput`
returns -1 for it. No other input is skipped or rewritten, and the firmware supports unterminated input on purpose (see the comment above the
loop), so this stays a real bug. `reproducer` is the one-byte input; to see the abort, delete the `rx_pending() == 1 && cmdLen == 0` line in
`firmware_loop()`, rebuild and run `./mayhem/_build/serial_fuzz-standalone reproducer`.
