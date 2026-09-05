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
