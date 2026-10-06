/*
  04_I2C_Scan.ino

  Ten-second "is the carrier alive?" check. No motors needed. Prints, once a
  second, every I2C address that ACKs plus the D11's raw firmware version string.

    0x66  D11 co-processor running motor firmware (healthy if fw starts with '0')
    0x09  D11 stuck in its bootloader -> run 02_Flasher_Reflash_D11
    0x6B  carrier's BQ24195 battery-charger chip (proves the carrier is connected)
    0x6A  Nano's own LSM6DS3 IMU
    0x60  Nano's own crypto chip
*/

#include <ArduinoMotorCarrier.h>

void setup() {
  Serial.begin(115200);
  Wire.begin();
}

void loop() {
  Serial.print("scan:");
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.print(" 0x"); Serial.print(a, HEX); }
  }
  Serial.print("  | fw=\"");
  Serial.print(controller.getFWVersion());
  Serial.println("\"");
  delay(1000);
}
