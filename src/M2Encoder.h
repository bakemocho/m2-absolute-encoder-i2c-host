#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "M2Client.h"
namespace m2enc {
inline uint32_t arduinoMicros() { return uint32_t(micros()); }
// Caller initializes Wire pins, clock and platform-specific timeout first.
class M2Encoder : public Client<TwoWire> {
public:
    explicit M2Encoder(TwoWire& bus=Wire, uint32_t (*clock)()=arduinoMicros,
                       uint8_t address=M2_ADDRESS):Client<TwoWire>(bus,clock,address) {}
};
} // namespace m2enc
