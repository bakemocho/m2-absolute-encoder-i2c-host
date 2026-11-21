# M2Encoder: I2C host library

[日本語の使い方](README.ja.md)

Version **0.3.0** targets **wire protocol 1 / sensor core 0x0E**.
0.3.0 adds `readEstimate`: sensors that have the optional estimate frame report
their best value at all times with a trust level (`Confirmed`, `Good`,
`Doubtful`, `Guess`). `poll` and every existing call are unchanged; see
[docs/protocol.md](docs/protocol.md#estimate-optional).
0.2.1 adds the `TooFast` core flag (0x80) and acquisition reason 7: the sensor
reports that the shaft outran its scan and withholds the angle until it slows.
It replaces the old 0x07 register reader; there is no backward-compatibility API.
Only the host interface is included. Sensor firmware, decoder and code patterns
are not part of this repository.

## Start with the monitor example

Initialize the platform's Wire pins, bus clock and bounded timeout, then call
`encoder.begin()`. It returns a `m2enc::Result`, **not a Boolean**:
`Ok` is zero. `begin()` reads the device version and command sequence; it does
not configure or start scanning.

See [basic_read.ino](examples/basic_read/basic_read.ino). It displays an
origin-adjusted angle from an already configured, running device and never
automatically starts scanning, moves a motor or changes persistent settings.

```cpp
m2enc::M2Encoder encoder; // Wire, micros(), 7-bit address 0x36
// After Wire initialization:
if (encoder.begin() == m2enc::Ok) {
    m2enc::Position p;
    float degrees;
    if (encoder.readPosition(p) == m2enc::Ok &&
        p.angleDegrees(micros(), degrees)) {
        // Use degrees here, while the sample remains fresh.
    }
}
```

`Reading` from `poll()` is in the raw coordinate system (1800 cells, 0.2°/cell).
`Position` applies the device's configured origin. Cell spacing and candidate
interval width are not claims of physical angular accuracy.

## API map

| Task | Methods |
| --- | --- |
| Read current angle/status | `poll`, `readPosition` |
| Best value with a trust level, before the angle is confirmed (optional frame) | `readEstimate` |
| Configure scan interval | `configureFixed`, `configureAuto`, `readConfiguration` |
| Explicit scan control | `start`, `stop` |
| Frozen candidate set/resolution | `captureCandidates`, `readQuality`, `readCandidatePage`, `readCandidateOrigin` |
| Persistent zero offset | `readOrigin`, `setOriginMilliDegrees`, `zeroHere` |
| Fault history | `readFaultMemory`, `clearFaultHistory` |
| Sensor response/individual repair | `testSensors`, `readMaintenance`, `readSensorTest`, `acknowledgeRepairedSensor` |
| Boot/reset diagnostics | `readBoardDiagnostic` |
| Uncertain command delivery | `pendingId`, `retry` |

The portable `Client<Bus>` takes a Wire-compatible bus and a
`uint32_t (*)()` microsecond clock. It does not allocate dynamically.
The Arduino `M2Encoder` wrapper supplies `TwoWire` and `micros()`.

## Commands: wait for a matching receipt

A command returning `Submitted` was acknowledged by I2C, not necessarily applied.
Save `pendingId()`, call `poll()` on later application iterations, and require
both a matching `command_id` and `command_result == M2_OK`.
A matching rejection also clears the pending command; inspect its result.

Only one command may be pending. If delivery is uncertain, `retry()` resends
the **same id and payload**. Set an application deadline; there is no infinite
retry loop in this library. `stop()` may supersede a pending command with a
new id, but is not a hardware emergency stop.

Use one controlling client per device. The 16-bit id is not a global transaction
service or a boot nonce. A previously observed nonzero receipt resetting to zero
is detected as `DeviceRestarted`; every possible reboot cannot be detected.
Communication failure invalidates that read, not the device's persistent state.
Recovery must not automatically erase history, move machinery or rearm outputs.

Configure before START. `Timing` holds settling time, blank time, stable-read
count and input polarity. Fixed/automatic periods are in microseconds.
Software ranges are in [the wire contract](docs/protocol.md); actual optical
settling and permissible rotation speed must be qualified for the installation.
No universal timing configuration is silently supplied.

## Freshness, candidates and storage

- Use `usable(micros())` or `angleDegrees(micros(), out)` at the point of use.
  All transaction latency is charged from the start of the read. Exact expiry
  is invalid. Use a wrapping 32-bit microsecond clock and do not retain samples
  for a full clock wrap.
- An accepted capture may still be building. Wait for complete Quality, start
  with a new `Candidates` object, then read all 12 pages from that capture.
  Different capture ids fail with `SnapshotChanged`. Historical bits may be
  inspectable after expiry, but only `usable()` authorizes current use.
- Candidate bits enumerate raw cells, not probabilities. `countWithin()` uses
  a raw-cell circular arc. `angleMilliDegrees()` combines a fresh candidate set
  with a matching `CandidateOrigin` snapshot. An IMU hint is host-side
  filtering only and does not change the encoder's lock or prove calibration.
- `Quality::resolutionDegrees()` describes the enclosing candidate-cell arc,
  including gaps; it does not mean all cells inside are possible.
- Origin/fault commands have separate **receipt** and **durable storage**
  completion. Read back the requested value/mask and durability before relying
  on it across power loss. `StorageTimeout` means the write outcome is
  uncertain, not that nothing was written.
- For replacement: explicitly STOP, run a digital sensor test, inspect the
  matching test id, then acknowledge physical repair for that sensor. Do not
  STOP between TEST and RESTORE: that invalidates the proof. Wait for
  `M2_MT_RESTORED` and the expected durable fault mask before START.
  A digital response is not an analog strength or full-range health measurement.
- `clearFaultHistory()` erases all learned exclusions. It is not the normal
  replacement procedure and cannot guarantee reacquisition with several
  unrepaired sensors.

## Electrical and timing boundary

The sensor is an I2C target at 7-bit address 0x36. Start evaluation at 100 kHz.
The current board implementation can stretch the clock; the host must support
that and bound its wait. Check the actual board's supply rails, logic levels,
pull-ups and shared-bus loading. This library neither adds pull-ups nor
qualifies 5 V tolerance, cable length, motor behavior or installation safety.

## Tests

```sh
python3 tests/run_tests.py
```

Requires Python 3 and a C++11 compiler with AddressSanitizer/UndefinedBehaviorSanitizer.
Tests cover CRC/endian helpers, command framing/receipts/retry, loss and
corruption, expiry/wrap, version rejection, candidates, origin, diagnostics
and invalid arguments. The monitor example was also compiled for M5Stack
Core ESP32 with espressif32 6.9.0 / Arduino-ESP32 2.0.17.

A local source-linked interoperability run exercised configuration, control,
persistence, maintenance and board-register behavior against core 0x0E in
default and cooperative build profiles. That proprietary test environment
is not included; the public host tests above are independently runnable.
No physical sensor/M5Stack, optical timing, cable or actuator was tested.

## License

MIT, for this host-side code only. Publishing this repository does not grant,
imply or waive patent rights in the sensor, coding scheme or sensor firmware.
