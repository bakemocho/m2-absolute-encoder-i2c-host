#include "M2Encoder.h"
#include <math.h>

namespace m2enc {

bool M2Encoder::begin(TwoWire& bus, uint8_t addr) {
  bus_ = &bus; addr_ = addr;
  uint8_t v = 0;
  return readReg(0x07, v);   // firmware version register answers on a live sensor
}

bool M2Encoder::readReg(uint8_t reg, uint8_t& value) {
  return readBurst(reg, &value, 1);
}

bool M2Encoder::readBurst(uint8_t start, uint8_t* buf, uint8_t n) {
  if (!bus_) return false;
  bus_->beginTransmission(addr_);
  bus_->write(start);
  if (bus_->endTransmission(false) != 0) return false;   // repeated start
  uint8_t got = bus_->requestFrom(addr_, n);
  if (got != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = (uint8_t)bus_->read();
  return true;
}

bool M2Encoder::read(Reading& out) {
  uint8_t b[0x17];
  if (!readBurst(0x00, b, sizeof b)) return false;
  out.cells      = (uint16_t)(b[0x00] | (b[0x01] << 8));
  out.status     = b[0x04];
  out.alive_mask = b[0x05];
  out.mark_width = b[0x06];
  out.fw_version = b[0x07];
  out.n_cand     = (uint16_t)(b[0x0E] | (b[0x0F] << 8));
  out.dead       = (uint16_t)(b[0x10] | (b[0x11] << 8));
  out.suspect    = (uint16_t)(b[0x14] | (b[0x15] << 8));
  out.dir        = b[0x16];
  out.degraded   = (out.status & ST_DEGRADED) != 0;
  out.valid      = (out.status & ST_ABSOLUTE) && !(out.status & ST_PROBATION) && out.cells != 0xFFFF;
  out.deg        = out.valid ? out.cells * 0.2f : NAN;
  if (out.status & ST_CFG_ERROR)      out.action = NextAction::CheckConfig;
  else if (!out.valid)                out.action = NextAction::Rotate;
  else if (out.dead || out.degraded)  out.action = NextAction::Service;
  else                                out.action = NextAction::Use;
  return true;
}

void M2Encoder::setSecondsPerCell(float s) { sec_per_cell_ = s; }

String M2Encoder::report(const Reading& r, uint32_t host_time) const {
  String s;
  if (host_time) { s += "["; s += host_time; s += "] "; }
  if (r.valid) { s += "angle "; s += r.deg; s += " deg"; }
  else         { s += "angle: not absolute (candidates "; s += r.n_cand; s += ")"; }
  switch (r.action) {
    case NextAction::Use:         s += " | ok"; break;
    case NextAction::Rotate:      s += " | rotate a little and re-read"; break;
    case NextAction::Service:     s += " | degraded, schedule service (dead mask 0x"; s += String(r.dead, HEX); s += ")"; break;
    case NextAction::CheckConfig: s += " | check mark-width setting"; break;
  }
  if (r.suspect) { s += " | probation mask 0x"; s += String(r.suspect, HEX); }
  return s;
}

}  // namespace m2enc
