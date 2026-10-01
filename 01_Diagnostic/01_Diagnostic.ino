/*
  01_Diagnostic.ino

  Standalone bench-test sketch for Nano 33 IoT + Arduino Nano Motor Carrier.
  No MATLAB, no WiFi, no JSON protocol -- just this sketch and the Arduino
  Serial Monitor (115200 baud). Flash this directly onto a suspect board to
  find out, layer by layer, what's actually broken:

    1. Nano's own I2C peripheral + onboard IMU  (sanity baseline)
    2. The carrier's D11 co-processor (I2C handshake + firmware version)
    3. Battery rail sensed by the carrier
    4. M1 / M2   -- driven THROUGH the D11 over I2C
    5. M3 / M4   -- driven DIRECTLY by Nano GPIO pins, no I2C, no D11
    6. Servo 1-4 -- driven THROUGH the D11 over I2C
    7. Encoder 1 / 2 -- read THROUGH the D11 over I2C

  Read the final summary block it prints. The M1/M2+servo vs M3/M4 split is
  the key: if M3/M4 work but M1/M2 and servos don't, the Nano itself is
  fine and the fault is the D11 co-processor or the I2C link to it (likely
  fix: re-flash the D11 with the sketch in 02_Flasher_Reflash_D11). If
  M3/M4 ALSO fail, look upstream of the carrier (power, seating, cabling).

  Each test is wrapped so a failure in one does not stop the others from
  running -- that's the whole point versus the class sketch, which bails
  out (or silently hangs) on the first problem it hits.
*/

#include <Wire.h>
#include <Arduino_LSM6DS3.h>
#include <ArduinoMotorCarrier.h>

bool imuOK = false;
bool carrierOK = false;
bool m1ok = false, m2ok = false, m3ok = false, m4ok = false;
bool s1ok = false, s2ok = false, s3ok = false, s4ok = false;
bool enc1ok = false, enc2ok = false;

void banner(const char* title) {
  Serial.println();
  Serial.println("==================================================");
  Serial.println(title);
  Serial.println("==================================================");
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  // Wait for the Serial Monitor, but don't hang forever if nobody opens one.
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 5000) {}
  delay(300);

  banner("NANO 33 IoT + MOTOR CARRIER DIAGNOSTIC");
  Serial.println("Starting tests. This will take about 15 seconds.");

  // ---- TEST 0: onboard IMU (sanity baseline for the Nano's own I2C) ----
  banner("TEST 0: Onboard IMU (LSM6DS3) -- baseline I2C sanity check");
  imuOK = IMU.begin();
  if (imuOK) {
    float x, y, z;
    while (!IMU.accelerationAvailable()) {}
    IMU.readAcceleration(x, y, z);
    Serial.println("PASS: IMU responded.");
    Serial.print("  accel x,y,z = ");
    Serial.print(x); Serial.print(", ");
    Serial.print(y); Serial.print(", ");
    Serial.println(z);
  } else {
    Serial.println("FAIL: IMU did not respond.");
    Serial.println("  This means the Nano's own I2C bus/pins are the problem --");
    Serial.println("  everything downstream (carrier included) is suspect too.");
  }

  // ---- TEST 1: D11 co-processor handshake ----
  banner("TEST 1: Motor Carrier D11 co-processor handshake");
  carrierOK = controller.begin();
  if (carrierOK) {
    Serial.print("PASS: Carrier responded. Firmware version: ");
    Serial.println(controller.getFWVersion());
    Serial.print("  Free RAM on D11: ");
    Serial.println(controller.getFreeRam());
    Serial.print("  D11 temperature (C): ");
    Serial.println(controller.getTemperature());
    controller.reboot();
    delay(500);
  } else {
    Serial.println("FAIL: Carrier did not respond.");
    Serial.println("  Look at the carrier board RIGHT NOW: is its red LED blinking?");
    Serial.println("  Blinking red LED = D11 is stuck in its bootloader, not running");
    Serial.println("  motor-control firmware. Fix: run the sketch in 02_Flasher_Reflash_D11.");
    Serial.println("  Everything that depends on the D11 (M1, M2, servos, encoders,");
    Serial.println("  battery sensing) will fail below. M3/M4 do NOT depend on the D11");
    Serial.println("  and are tested independently -- watch those to see if the Nano");
    Serial.println("  itself is fine.");
  }

  // ---- TEST 2: Battery rail (only meaningful if carrier responded) ----
  if (carrierOK) {
    banner("TEST 2: Battery rail (read through the D11)");
    int raw = battery.getRaw();
    float volts = raw / 236.0; // 236 counts/volt is the Nano carrier's documented scale factor
    Serial.print("  raw = "); Serial.print(raw);
    Serial.print("  ~volts = "); Serial.println(volts, 3);
    if (raw < 50) {
      Serial.println("  NOTE: that reads as near-zero. Is the LiPo plugged in and the");
      Serial.println("  carrier's power switch ON? M1/M2/servos need that rail, not USB.");
    } else {
      Serial.println("PASS: carrier is sensing a battery voltage.");
    }
  }

  // ---- TEST 3: M1 / M2 (through the D11, over I2C) ----
  banner("TEST 3: M1 / M2 -- driven THROUGH the D11 over I2C");
  if (!carrierOK) {
    Serial.println("SKIPPED: carrier handshake failed above, these can't work yet.");
  } else {
    m1ok = spinAndAsk(M1, "M1");
    m2ok = spinAndAsk(M2, "M2");
  }

  // ---- TEST 4: M3 / M4 (direct GPIO, no D11 involved) ----
  banner("TEST 4: M3 / M4 -- driven DIRECTLY by Nano GPIO pins (no I2C/D11)");
  Serial.println("These should work even if TEST 1 failed. If they also fail,");
  Serial.println("the problem is upstream of the carrier (power/wiring/the Nano itself).");
  m3ok = spinAndAsk(M3, "M3");
  m4ok = spinAndAsk(M4, "M4");

  // ---- TEST 5: Servos (through the D11, over I2C) ----
  banner("TEST 5: Servo 1-4 -- driven THROUGH the D11 over I2C");
  if (!carrierOK) {
    Serial.println("SKIPPED: carrier handshake failed above, these can't work yet.");
  } else {
    s1ok = sweepAndAsk(servo1, "servo1");
    s2ok = sweepAndAsk(servo2, "servo2");
    s3ok = sweepAndAsk(servo3, "servo3");
    s4ok = sweepAndAsk(servo4, "servo4");
  }

  // ---- TEST 6: Encoders (through the D11, over I2C) ----
  banner("TEST 6: Encoder 1 / 2 -- read THROUGH the D11 over I2C");
  if (!carrierOK) {
    Serial.println("SKIPPED: carrier handshake failed above, these can't work yet.");
  } else {
    enc1ok = encoderAndAsk(encoder1, M1, "encoder1 / M1");
    enc2ok = encoderAndAsk(encoder2, M2, "encoder2 / M2");
  }

  // ---- Summary ----
  banner("SUMMARY");
  printResult("IMU (baseline)", imuOK);
  printResult("Carrier/D11 handshake", carrierOK);
  printResult("M1 (via D11)", m1ok);
  printResult("M2 (via D11)", m2ok);
  printResult("M3 (direct GPIO)", m3ok);
  printResult("M4 (direct GPIO)", m4ok);
  printResult("servo1 (via D11)", s1ok);
  printResult("servo2 (via D11)", s2ok);
  printResult("servo3 (via D11)", s3ok);
  printResult("servo4 (via D11)", s4ok);
  printResult("encoder1 (via D11)", enc1ok);
  printResult("encoder2 (via D11)", enc2ok);

  Serial.println();
  if (!imuOK) {
    Serial.println("-> IMU failed: suspect the Nano board itself, not the carrier.");
  } else if (!carrierOK && (m3ok || m4ok)) {
    Serial.println("-> Nano is fine (IMU + M3/M4 OK), but the D11 is dead.");
    Serial.println("   Go run 02_Flasher_Reflash_D11 on this board.");
  } else if (!carrierOK && !m3ok && !m4ok) {
    Serial.println("-> Nothing on the carrier works, not even the direct-GPIO motors.");
    Serial.println("   Check carrier seating on the header pins, and LiPo power/switch,");
    Serial.println("   before assuming it's a firmware problem.");
  } else if (carrierOK && (!m1ok || !m2ok) && m3ok && m4ok) {
    Serial.println("-> D11 is alive, but one specific I2C motor channel failed.");
    Serial.println("   That's most likely a mechanical/solder fault on that one H-bridge");
    Serial.println("   channel on the carrier, not a firmware issue.");
  } else if (carrierOK && m1ok && m2ok && m3ok && m4ok) {
    Serial.println("-> Everything passed. This board/carrier pair is healthy.");
  }

  Serial.println();
  Serial.println("Done. Re-run (reset the board) to test again.");
}

void loop() {
  // Nothing to do -- all tests run once in setup().
}

void printResult(const char* name, bool ok) {
  Serial.print("  ");
  Serial.print(name);
  Serial.print(": ");
  Serial.println(ok ? "PASS" : "FAIL/SKIPPED");
}

// Spins a DC motor forward then backward briefly and asks the human to confirm.
bool spinAndAsk(mc::DCMotor &motor, const char* name) {
  return spinGeneric(motor, name);
}
bool spinAndAsk(d21::DCMotor &motor, const char* name) {
  return spinGeneric(motor, name);
}

template <typename MotorT>
bool spinGeneric(MotorT &motor, const char* name) {
  Serial.print("  Spinning "); Serial.print(name); Serial.println(" forward...");
  motor.setDuty(40);
  delay(800);
  Serial.print("  Spinning "); Serial.print(name); Serial.println(" backward...");
  motor.setDuty(-40);
  delay(800);
  motor.setDuty(0);
  Serial.print("  Did "); Serial.print(name); Serial.println(" visibly move both directions? (y/n, 5s timeout = assume no)");
  return waitYesNo();
}

bool sweepAndAsk(mc::ServoMotor &servo, const char* name) {
  Serial.print("  Sweeping "); Serial.print(name); Serial.println(" 0 -> 180 -> 90...");
  for (int a = 0; a <= 180; a += 20) { servo.setAngle(a); delay(60); }
  for (int a = 180; a >= 90; a -= 20) { servo.setAngle(a); delay(60); }
  Serial.print("  Did "); Serial.print(name); Serial.println(" visibly move? (y/n, 5s timeout = assume no)");
  return waitYesNo();
}

bool encoderAndAsk(mc::Encoder &enc, mc::DCMotor &motor, const char* name) {
  enc.resetCounter(0);
  motor.setDuty(40);
  delay(1000);
  motor.setDuty(0);
  long counts = enc.getRawCount();
  Serial.print("  "); Serial.print(name); Serial.print(" counts after 1s spin: "); Serial.println(counts);
  bool ok = counts != 0;
  Serial.println(ok ? "PASS (counter moved)" : "FAIL (counter stayed at 0)");
  return ok;
}

// Reads a single y/n line from Serial, 5s timeout -> treated as 'n'.
bool waitYesNo() {
  unsigned long t0 = millis();
  while (millis() - t0 < 5000) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      while (Serial.available() > 0) Serial.read(); // flush rest of line
      return (c == 'y' || c == 'Y');
    }
  }
  Serial.println("  (no response, timed out -> counted as 'no')");
  return false;
}
