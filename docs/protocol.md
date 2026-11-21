# Protocol 1 / core 0x0E host contract

Use the typed client unless implementing another host transport.
All multibyte integers are unsigned little-endian. The 7-bit address is 0x36.
One selector byte followed by a repeated START reads one 32-byte response.
There is no register auto-increment across response types. The board may
stretch SCL; use a bounded host timeout.

Every response has its type magic at byte 0, protocol 1 at byte 1, and
CRC-8/SMBUS over bytes 0..30 at byte 31. The CRC polynomial is 0x07, initial
value 0, no reflection and no final XOR. This is payload CRC, not
address-inclusive SMBus PEC. The check value for ASCII `123456789` is 0xF4.

## Selectors

| Selector | Magic | Response |
| --- | --- | --- |
| 0x80 | 0xE1 | Status, command receipt and raw position |
| 0x81 | 0xE2 | Scan configuration |
| 0x82 | 0xE4 | Candidate quality and fault masks |
| 0x83 | 0xE5 | Fault persistence |
| 0x84 | 0xE6 | Board diagnostics; boot extension revision 1 |
| 0x85 | 0xE7 | Persistent origin |
| 0x86 | 0xE8 | Origin-adjusted position |
| 0x87 | 0xE9 | Origin bound to a candidate capture |
| 0x88 | 0xEA | Maintenance progress, revision 1 |
| 0x89 | 0xEC | Estimate with a trust level, revision 1; optional |
| 0x90..0x9B | 0xE3 | Candidate pages 0..11 |
| 0xB0..0xB9 | 0xEB | Digital test results for sensors 0..9 |

The client checks type, protocol and CRC. `begin()` additionally requires
core version 0x0E from Status byte 24. Unsupported hardware extensions return
an error rather than a cached successful diagnostic.

## Estimate (optional)

Some sensors answer selector 0x89. Others refuse it, and the read then fails
with `CorruptResponse` or `BusError`; that is not a fault of the sensor. The
Status frame is the same on both, so `poll()` works unchanged.

The frame carries the sensor's best value at all times, with a level:

| Level | Name | Meaning for the host |
| --- | --- | --- |
| 4 | `Confirmed` | The angle of the Status frame. Same cell in both frames. |
| 3 | `Good` | Not yet confirmed, and no reason for doubt is known to the sensor. |
| 2 | `Doubtful` | Right while the shaft is slow; can be far off after the shaft outran the sensor. |
| 1 | `Guess` | More often wrong than right. |
| 0 | `NoEstimate` | No value. |

Only `Confirmed` is a position that anything may depend on. The lower levels
exist so that a host can show a value, choose a direction, or decide how
carefully to move while it waits for `Confirmed`.

| Bytes | Field |
| --- | --- |
| 2 | Frame revision, 1 |
| 3 | Sensor core identifier (informational) |
| 4..5 | Cell, 0..1799; 0xFFFF with level 0 |
| 6 | Level |
| 7 | Margin (informational, larger is better) |
| 8..9 | Detail word of the sensor (see below) |
| 10..11 | Confirmed cell as in Status; 0xFFFF unless level 4 |
| 12..15 | Frame sequence |
| 16..19 | Age of the oldest sensor read in microseconds; 0xFFFFFFFF if not all sensors were read |
| 20..23 | Time the confirmed angle stays valid, as in Status |
| 24..26 | Core version, flags and core status as in Status bytes 24, 25 and 7 |

The client refuses a frame whose level and cells contradict each other. It
folds the detail word into four cautions and keeps the word itself as
`Estimate::diagnostic`. Its bits are not part of the contract; quote the word
in a support request.

| Caution | Meaning |
| --- | --- |
| `ShaftFast` | The shaft moved faster than the sensor can follow, now or just before. |
| `SensorDoubt` | A sensor read is missing, stale, excluded or disagrees with the others. |
| `InstallationCheck` | The sensor has not finished checking its installation. Turn the shaft slowly through a wide arc once. |
| `Settling` | The value has not settled; it needs more travel. |

`Estimate::atLeast(level, now, max_age)` is true when the level is reached and
the sensor reads behind the value are younger than `max_age`, counting the
time since the bus transaction started.

## Status

| Offset | Bytes | Field |
| --- | --- | --- |
| 2 | 2 | Receipt command id |
| 4 | 1 | Command result; 0 is accepted |
| 5 | 1 | Active mode: 0 stopped, 3 fixed, 4 automatic |
| 6 | 1 | Acquisition state; 2 tracking, 3 stopped |
| 7 | 1 | Core status flags |
| 8 | 2 | Raw cell 0..1799, otherwise invalid |
| 10 | 4 | Input age, microseconds |
| 14 | 4 | Scan period, microseconds |
| 18 | 4 | Sample sequence |
| 22 | 2 | Last processed command id |
| 24 | 1 | Core version |
| 25 | 1 | Flags: valid 1, configured 2, overrun 4, board command pending 8 |
| 26 | 4 | Remaining validity, microseconds |
| 30 | 1 | Acquisition reason |

Core flags: Absolute 0x01, Degraded 0x02, NeedMotion 0x04,
ConfigurationError 0x08, BitFault 0x10, Probation 0x20, Reversed 0x40,
TooFast 0x80. TooFast means a sensor edge passed faster than the configured
stable-read window could commit it: the shaft is turning faster than the scan
can follow, NeedMotion is set as well, and the angle is withheld until the
shaft slows. Acquisition reason 7 (too fast) accompanies it.
An I2C/CRC success does not by itself make the angle valid. Check the
acquisition state, flags, range and remaining time through the typed API.

Position 0x86 carries flags at 2 (valid 1, origin-ready 2, origin-busy 4),
raw cell at 4, angle in millidegrees at 6, origin offset at 10, origin
generation at 14, remaining validity at 18 and sequence at 22.
The angle is `(raw_cell * 200 + 360000 - origin_offset) % 360000`.

## Commands

Write selector 0xA0 and exactly 24 payload bytes in one transaction with STOP:

| Payload offset | Bytes | Field |
| --- | --- | --- |
| 0 | 1 | Protocol 1 |
| 1 | 2 | Nonzero command id |
| 3 | 1 | Operation |
| 4..21 | 18 | Operation arguments; unused bytes zero |
| 22 | 1 | Reserved, zero |
| 23 | 1 | CRC over payload bytes 0..22 |

Operations: STOP 1, START 2, FIXED 3, AUTO 4, CAPTURE 5,
CLEAR_FAULTS 6, SET_ORIGIN 7, ZERO_HERE 8, TEST_SENSORS 9,
RESTORE_SENSOR 10.

FIXED/AUTO arguments: minimum period at 4, maximum at 8, settling at 12,
blanking at 16 (all four-byte microseconds), stable reads at 20,
inverted-input Boolean at 21. FIXED requires equal minimum and maximum.
Software validation requires minimum >=1000, maximum <=1000000,
minimum <= maximum, settling/blanking each 10..10000, stable reads 2..32,
and `10 * (settling + blanking) <= minimum`.
These are software argument limits, not qualified optical or speed limits.

SET_ORIGIN takes a four-byte offset 0..359999 millidegrees at 4.
TEST_SENSORS takes a nonzero ten-bit mask at 4 (two bytes) and cycles 2..32
at 6. RESTORE_SENSOR takes sensor index 0..9 at 4, explicit repair
acknowledgement 0xA5 at 5, and the successful test id at 6 (two bytes).
The device checks stopped state, test evidence and storage readiness.

Command results are enumerated in `M2Protocol.h`. Rejection does not mean a
transport failure. Storage operations also require separate durable readback.

## Candidates and time

Capture is asynchronous. Quality identifies a completed capture; it reports
candidate count, confirmed/suspect/live sensor masks and an enclosing circular
interval. A candidate bitmap has 225 bytes for 1800 raw cells. Pages contain
19 bytes each, except page 11 with 16 bytes, beginning at response offset 12.
Page index is at 2, byte count at 3, capture id at 4 and remaining time at 8.
Bit `cell % 8` in byte `cell / 8` denotes membership. Require all 12 pages,
matching capture ids and unexpired quality before current use.

All host freshness checks use unsigned 32-bit elapsed microseconds, charge
the read from its start, reject zero/half-range-or-longer lifetimes and expire
at equality. No response establishes a physical measurement or a universal
reboot identity. A fresh CRC validates the received frame, not all upstream
sensor electronics or a frozen-but-plausible faulty device.
