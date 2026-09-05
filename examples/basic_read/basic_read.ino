#include <M2Encoder.h>

m2enc::M2Encoder enc;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  if (!enc.begin(Wire, 0x36)) Serial.println("sensor not found");
}

void loop() {
  m2enc::Reading r;
  if (enc.read(r)) Serial.println(enc.report(r));
  delay(50);   // 10-100 ms polling is plenty; the sensor updates internally at 1 ms
}
