// Monitor a device configured and started by its application.
// Never starts scanning/motion or writes persistent settings automatically.
#include <M2Encoder.h>
m2enc::M2Encoder encoder;
bool attached = false;

void setup() {
  Serial.begin(115200);
  Wire.begin(); // Select the correct SDA/SCL pins for your board first.
  Wire.setClock(100000);
#if defined(ARDUINO_ARCH_ESP32)
  Wire.setTimeOut(30);
#elif defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(30000, true);
#endif
  // Other platforms must supply their own bounded Wire timeout.
  attached = encoder.begin() == m2enc::Ok;
  if (!attached) Serial.println("No compatible device; check wiring and restart explicitly.");
}

void loop() {
  if (!attached) { delay(100); return; }
  m2enc::Reading status;
  m2enc::Result result = encoder.poll(status);
  if (result != m2enc::Ok) {
    Serial.print("Read failed: "); Serial.println(int(result));
    if (result == m2enc::DeviceRestarted || result == m2enc::UnsupportedProtocol) attached = false;
    delay(50); return;
  }
  m2enc::Position position;
  float degrees;
  if (status.usable(micros()) && encoder.readPosition(position) == m2enc::Ok &&
      position.angleDegrees(micros(), degrees)) {
    Serial.print("Angle: "); Serial.print(degrees, 1);
    Serial.println((status.core_status & m2enc::Degraded) ? " deg (service required)" : " deg");
  } else {
    Serial.println("No current angle: check scan configuration and acquisition state.");
  }
  delay(50); // Host polling policy, not the sensor's scan period.
}
