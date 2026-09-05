// M2Encoder.h — host-side reader for the M2 absolute angle sensor
// (I2C slave, default address 0x36).  Arduino-compatible (Wire).
//
// This library reads the sensor's register file and turns it into a small
// status structure.  It contains no part of the sensor itself: not the code
// plate pattern, not the decoder, not the sensor firmware.
//
// Register map: firmware 0x07 (see README.md, "Register map").
#pragma once
#include <Arduino.h>
#include <Wire.h>

namespace m2enc {

// Status register (0x04) bits.
enum StatusBit : uint8_t {
  ST_ABSOLUTE    = 0x01,  // angle is unique
  ST_DEGRADED    = 0x02,  // running with contacts/modules excluded
  ST_NEED_MOTION = 0x04,  // not enough evidence yet; angle reads 0xFFFF
  ST_CFG_ERROR   = 0x08,  // mark-width setting invalid
  ST_BIT_FAULT   = 0x10,  // a stuck contact was identified and excluded
  ST_PROBATION   = 0x20,  // provisional exclusion active; angle not release-grade
  ST_REVERSED    = 0x40   // last event was in the reverse direction (informational)
};

enum class NextAction : uint8_t {
  Use = 0,          // angle valid, use it
  Rotate = 1,       // evidence missing; rotate a little (either direction) and re-read
  Service = 2,      // degraded; schedule contact replacement (see `dead`)
  CheckConfig = 3   // mark-width setting w invalid
};

struct Reading {
  uint16_t cells;      // 0..1799 (0.2 deg per cell); 0xFFFF = not absolute
  float    deg;        // cells * 0.2f, NAN when !valid
  bool     valid;      // ST_ABSOLUTE set and ST_PROBATION clear
  uint8_t  status;     // raw status byte (0x04)
  bool     degraded;   // ST_DEGRADED
  uint16_t dead;       // confirmed stuck contacts, bit g = contact g (0x10-0x11)
  uint16_t suspect;    // provisional exclusions (0x14-0x15)
  uint16_t n_cand;     // position candidates; 1 = unique (0x0E-0x0F)
  uint8_t  dir;        // 0 unknown / 1 forward / 2 backward / 3 mixed (0x16)
  uint8_t  alive_mask; // module alive mask (0x05), ring variant only
  uint8_t  mark_width; // configured mark width w (0x06)
  uint8_t  fw_version; // 0x07
  NextAction action;   // derived on the host from status (see README)
};

class M2Encoder {
 public:
  bool begin(TwoWire& bus = Wire, uint8_t addr = 0x36);

  // Reads registers 0x00..0x16 in one burst and fills `out`.
  // Returns false on a bus error.
  bool read(Reading& out);

  // Host-known rotation rate, used only to convert cells to time in report().
  void setSecondsPerCell(float s);

  // Human-readable one-line status for logs.  `host_time` is the host clock
  // (the sensor has none); pass 0 to omit timestamps.
  String report(const Reading& r, uint32_t host_time = 0) const;

  // Raw access, for diagnostics.
  bool readReg(uint8_t reg, uint8_t& value);
  bool readBurst(uint8_t start, uint8_t* buf, uint8_t n);

 private:
  TwoWire* bus_ = nullptr;
  uint8_t  addr_ = 0x36;
  float    sec_per_cell_ = NAN;
};

}  // namespace m2enc
