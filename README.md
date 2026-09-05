# M2Encoder: host-side I2C reader for the M2 absolute angle sensor

日本語版は [README.ja.md](README.ja.md) にあります。

Arduino-compatible library for reading the M2 absolute angle sensor over I2C.
"M2" is the name of this encoder series. The sensor resolves absolute angle from a coded plate mounted on the
rotating body itself, so the origin belongs to the machine and does not reset
when a motor, coupling or reducer is replaced.

This repository is the host side only. It reads the sensor's register file and
presents angle, validity, degradation state and a suggested next action.

It contains no part of the sensor itself. The coded plate pattern, the decoder
and the sensor firmware are not published here.

## Wiring

| Item | Value |
|---|---|
| Role | sensor is the I2C **slave**; the host is the master |
| Address | **0x36** (7-bit) |
| Speed | 100 kHz (standard mode) |
| Supply | 3.3 V (optical variant); pull-ups are on the sensor board; do not add more |
| Clock stretching | none; every read answers immediately |

## Reading

Write the register address, repeated start, then read. The address
auto-increments, so the library reads `0x00..0x16` in one burst.

```cpp
m2enc::M2Encoder enc;
enc.begin(Wire, 0x36);
m2enc::Reading r;
if (enc.read(r) && r.valid) use(r.deg);   // 0.0 .. 359.8 in 0.2° steps
```

`r.valid` is true only when the status has ABSOLUTE set and PROBATION clear.
A reading of `0xFFFF` cells means "not absolute yet". It is not a number, so do
not average it.

## Example

The sketch below is `examples/basic_read/basic_read.ino`. It reads the sensor
every 50 ms and branches on the suggested action.

```cpp
// basic_read: read the M2 absolute angle sensor and act on its status.
#include <Wire.h>
#include <M2Encoder.h>

m2enc::M2Encoder enc;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  if (!enc.begin(Wire, 0x36)) {
    Serial.println("sensor not found at 0x36");
  }
  enc.setSecondsPerCell(0.033f);   // your rotation rate; only used by report()
}

void loop() {
  m2enc::Reading r;
  if (!enc.read(r)) {
    Serial.println("bus error");
    delay(100);
    return;
  }

  switch (r.action) {
    case m2enc::NextAction::Use:
      Serial.print("angle "); Serial.print(r.deg, 1); Serial.println(" deg");
      break;
    case m2enc::NextAction::Rotate:
      // Angle is 0xFFFF until the sensor has seen a little motion (worst case 0.8 deg).
      Serial.print("not absolute yet, candidates "); Serial.println(r.n_cand);
      break;
    case m2enc::NextAction::Service:
      // Angle is still valid; one or more contacts are excluded. Plan maintenance.
      Serial.print("angle "); Serial.print(r.deg, 1);
      Serial.print(" deg (degraded, dead mask 0x"); Serial.print(r.dead, HEX); Serial.println(")");
      break;
    case m2enc::NextAction::CheckConfig:
      Serial.println("mark-width setting invalid; check installation");
      break;
  }
  delay(50);   // 10-100 ms polling is enough; the sensor updates internally at 1 ms
}
```

`r.action` is derived on the host from the status byte. It is `Use` when the
angle is valid and nothing is excluded, `Rotate` while the sensor still needs
motion, `Service` when the angle is valid but contacts are excluded, and
`CheckConfig` on a configuration error.

## Register map (firmware 0x07)

| Addr | Bytes | Content |
|---|---|---|
| 0x00–0x01 | 2 | Angle, cells 0–1799 (×0.2 = degrees), little-endian; 0xFFFF = not absolute |
| 0x02–0x03 | 2 | Raw 15-bit sensor word (debug) |
| 0x04 | 1 | Status flags (below) |
| 0x05 | 1 | Module alive mask (bits 0–2, ring variant) |
| 0x06 | 1 | Configured mark width w |
| 0x07 | 1 | Firmware version (0x07) |
| 0x08–0x0D | 6 | Debug: raw input, debounced input, reset count, I2C error count, role, module id |
| 0x0E–0x0F | 2 | Position candidate count; 1 = unique |
| 0x10–0x11 | 2 | Confirmed stuck-contact bitmap |
| 0x12 | 1 | Decoder contradiction count |
| 0x13 | 1 | Self-repair adoption count |
| 0x14–0x15 | 2 | Provisional exclusion (probation) bitmap |
| 0x16 | 1 | Direction of the last event: 0 unknown / 1 forward / 2 backward / 3 mixed |
| other | 1 | 0xEE |

### Status flags (0x04)

| Bit | Name | Meaning | Suggested host action |
|---|---|---|---|
| 0x01 | ABSOLUTE | angle is unique | use the angle |
| 0x02 | DEGRADED | contacts or a module excluded | keep using; schedule service |
| 0x04 | NEED_MOTION | not enough evidence; angle is 0xFFFF | rotate a little and re-read |
| 0x08 | CFG_ERROR | mark-width setting invalid | check installation setting |
| 0x10 | BIT_FAULT | a stuck contact was identified and excluded | schedule replacement |
| 0x20 | PROBATION | provisional exclusion; angle not release-grade | do not use the angle |
| 0x40 | REVERSED | last event was in reverse | informational |

## Behaviour to plan for

- Power-up always starts at 0xFFFF / NEED_MOTION. No calibration is needed,
  but a small rotation is: worst case 4 cells (0.8°) when healthy.
- Poll every 10–100 ms. The sensor updates internally at 1 ms; polling faster
  gains nothing.
- Read 0x00–0x01 as one 2-byte burst so both bytes come from the same moment.
- Rotation-rate limit of the current read method: about 2 revolutions per
  minute.

## License

MIT. The license covers this host-side code only. Publishing this repository
does not grant, imply, or waive any patent license: no rights in the sensor,
its coding scheme, or its firmware are conveyed.
