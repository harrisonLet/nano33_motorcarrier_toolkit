# Nano 33 IoT + Motor Carrier Toolkit (ECE 3610)

Board: `arduino:samd:nano_33_iot`. All sketches use the libraries in `nanobot_edu/libraries/`.

> **⚠ Most important: Nano pins D2, D3, D4, D5 ARE the M3/M4 motor controls.**
> They're wired straight into the M3 (D2/D3) and M4 (D4/D5) driver inputs on the carrier.
> Any `digitalWrite`, ultrasonic, `pinMode`, etc. on those pins **directly drives M3/M4**:
> the motor terminal LEDs light, and anything plugged into M3/M4 moves. Conversely, setting
> M3/M4 changes what a sensor on those pins sees. This explains a lot of "random"
> behavior: M3/M4 LEDs lighting when using other parts, motors twitching, sensors
> reading garbage. Rule: **if a sensor is on D2–D5, nothing goes on M3/M4, and vice versa.**
> The course's final-project scripts put ultrasonics on exactly these pins. Full pin list
> under "Nano pins the carrier uses" below.

## Before anything else: reading the board

- "Uploading a sketch" = flashing firmware with the Arduino IDE / `arduino-cli`. TA-only.
  Students' MATLAB scripts never flash anything. `nanobot(...)` just sends JSON over serial
  to the firmware already on the board.
- **LED doing a smooth fade in/out = bootloader** (double-tapped reset, waiting to be flashed).
  MATLAB still sees a port, but nothing on the board answers, so `nanobot(...)` hangs.
  **Press reset once** to go back to the sketch. Entering/leaving the bootloader fixes nothing
  by itself. It's only for flashing.
- Sharp on/off blinks are the sketch itself (see the blink codes under
  `03_Hardened_Class_Sketch`, or `nb.ledWrite`).
- **Single reset or replug** = restart the sketch and re-initialize the carrier. It's the first
  thing to try when a board misbehaves. The port disappears and comes back, so the student must
  `clear` their old `nanobot` object and re-run `nanobot(...)`. Otherwise it looks like the
  reset didn't help.

## Fixing a "broken" board

Go top to bottom and stop at the first step that fixes it.

1. **Port doesn't show up, or vanishes after an upload:** check the connection path first.
   - Don't use an adapter or dock with a **USB 2.0 hub** inside. The hub drops the board when it
     resets at the end of an upload, and only a physical replug brings it back. To check, look
     for "USB2.0 Hub" above the Arduino in System Information → USB (Mac) or Device Manager
     (Windows). Apple's USB-C Digital AV Multiport Adapter is one of these.
   - Try a different cable. Charge-only micro-USB cables are common and carry no data.
2. **Still no port on a direct connection:** double-tap the Nano's reset button. The LED
   pulses and the bootloader's port appears. Upload any sketch to that port. A bad sketch
   can't damage the bootloader, so this recovers almost every "dead" Nano.
3. **Double-tap gives no port either:** the bootloader is corrupted. Reflashing it needs an
   SWD programmer (J-Link / Atmel-ICE). It's rare, so set the board aside.
4. **Nano works but M1/M2/servos/encoders don't:** check the carrier with `04_I2C_Scan`
   (battery plugged in, switch ON):

   | Scan shows | Meaning | Fix |
   |---|---|---|
   | `0x66`, fw starts with `0` | D11 healthy | nothing |
   | `0x09`, no `0x66` (red/amber LED lit) | D11 stuck in bootloader | `02_Flasher_Reflash_D11` |
   | `0x66`, fw doesn't start with `0` | D11 has wrong firmware | `02_Flasher_Reflash_D11` |
   | no `0x66` / `0x09` / `0x6B` | carrier not connected or unpowered | reseat it, check battery + switch |

5. **Nothing works, even M3/M4, with the carrier seated and powered:** probably physical damage
   (bent header pin, blown driver). Swap parts to find which board is bad.

Once it works, flash `03_Hardened_Class_Sketch` onto it.

## How the Motor Carrier works

The carrier has its own microcontroller, an ATSAMD11 (the "D11"), with its own firmware. The
Nano controls it over I2C at address `0x66`. **Some ports go through the D11 and some don't:**

| Port | Controlled by | Works if the D11 is dead? |
|---|---|---|
| M1, M2 | D11 (over I2C) | No |
| M3, M4 | Nano pins directly (D2/D3, D5/D4) | **Yes** |
| Servo 1–4 | D11 | No |
| Encoder 1, 2 | D11 (it does the counting) | No |
| Battery voltage reading | D11 | No |
| IN1–IN4 sensor ports | Nano analog pins (A7, A2, A6, A3) | Yes |

So if M3/M4 work but M1/M2 and the servos don't, the Nano is fine and the D11 is the problem.

### Nano pins the carrier uses

Every Nano pin is wired to something on the carrier whenever the Nano is plugged in, whether
or not you use that feature. From Arduino's schematic (ABX00041):

| Nano pin | Wired to on the carrier | Free to use? |
|---|---|---|
| D0 / RX | BNO055 boot-mode pin (10k pull-up) | Avoid. The BNO055 didn't answer our I2C scan, so it may not be fitted. |
| D1 / TX | nothing | **Yes** |
| **D2, D3** | **M3 driver inputs** | **No** if M3 is used. Any signal here drives M3. |
| **D4, D5** | **M4 driver inputs** | **No** if M4 is used. Any signal here drives M4. |
| D6 | D11 interrupt line | **No** |
| D7, D8 | nothing | **Yes** |
| D9 | BNO055 interrupt output | Probably (BNO055 didn't answer our scan) |
| D10, D11 | nothing | **Yes** |
| **D12** | **shared motor FAULT line** (all 4 drivers) + fault LED | Avoid. Driving it LOW lights the fault LED. A real driver fault pulls it LOW. |
| D13 | nothing (onboard LED) | **Yes** |
| A0 (= D14) | M3 current-sense output | With care. See note below. |
| A1 (= D15) | M4 current-sense output | With care. See note below. |
| A2, A3, A6, A7 | the IN2, IN4, IN3, IN1 sensor ports | Use them *through* those ports |
| A4, A5 | I2C (D11, charger chip) | **No** |
| VIN | carrier's ~12 V M1/M3 motor rail | Don't connect anything |
| VUSB | carrier's charger input | Don't connect anything |

**Truly free with the carrier attached: D1, D7, D8, D10, D11, D13** (plus D9/D0 with care),
and the four IN ports for sensors. (A0 and D14 are the same physical pin, two names. Same
for A1/D15.)

**A0/A1 note:** the motor driver's current-sense pin (VISEN) *sources* about 0.68 V per amp
of motor current (6.81 kΩ ISET, MP6522 datasheet), up to ~2 mA. With M3/M4 off or no motor
attached it sits at ~0 V and a sensor/voltage divider on A0/A1 reads normally. While M3/M4
draw real current (stall, startup) it can pull a low reading *up*. A stiff, low-resistance
divider is affected less. The link is a 0 Ω resistor that may not be fitted on every
carrier. Not yet bench-measured.

Clashes in the current course code:
- `final/control.m`, `newmain.m`, `rangefind.m` put ultrasonics on **D2/D3 and D4/D5**. OK
  while M3/M4 are unused, but every ping also drives M3/M4: their LEDs flash and anything
  plugged in twitches. Move them to D7/D8 and D10/D11 if M3/M4 are ever needed.
- `labs/lab6.m` puts an ultrasonic on **D14/D15 (= A0/A1)**, the M3/M4 current-sense
  pins. Fine while M3/M4 are idle. With M3/M4 running, the echo/trigger lines share a pin
  with the driver's sense output.
- `labs/lab3.m` / `lab6.m` use **D12** for the RGB LED (red). With the carrier attached,
  that also lights the carrier's fault LED, which is misleading but harmless.
- The piezo's `tone()` uses the same internal timer (TC5) as M3's pins, so piezo and M3
  interfere.

**IMU.** The class sketch reads the Nano's own LSM6DS3 at `0x6A`. The carrier also lists a
BNO055 IMU, but the class code doesn't use it.

**Carrier LEDs** (from Arduino's schematic, ABX00041):
- **One LED per motor terminal (M1+ M1- … M4+ M4-):** lit = the driver is pushing that
  terminal high (to ~12 V). Brightness tracks duty cycle. + lit = one direction, - lit = the
  other. They light with or without a motor attached, so they show what the driver is
  *told* to do, not whether the motor moves. A port lighting when you didn't command it
  means its input pins are being driven by something else (see the D2–D5 note above).
- **Green "ON" (by the switch):** battery connected and switch ON.
- **Red/amber (next to the green):** lit = D11 stuck in its bootloader. Reflash it. Off =
  D11 running normally. (Seen lit on a stuck carrier and off after reflashing.)
- **CHRG:** solid = charging the battery from USB. Off = full or no USB. Blinking ~1/s =
  charge fault, so swap the battery.

### Power

- **Battery:** 1-cell (1S) Li-ion/LiPo only, 4 V max, via the XT-30 or 2-pin terminal block.
  Arduino's setup instructions say to plug in the battery and turn the switch ON before using
  motors or servos. Treat both as required.
- **Motor power comes only from the battery, through the switch.** Two boost converters
  raise the battery to ~12 V: one feeds **M1 + M3**, the other **M2 + M4**. With the battery
  out or the switch OFF there is no motor power at all, even on USB. If M1 and M3 die
  together (or M2 and M4), suspect that pair's boost converter rather than the drivers.
- **Motor drivers:** four MP6522s, 500 mA max each, with over-current and over-temperature
  shutdown. A stalled or overloaded motor can **cut out and come back on its own**. That's a
  common cause of "motor works intermittently."
- **Servos run on 5 V** from USB when USB is plugged in, or boosted from the battery when it
  isn't. So servos can move on USB alone, drawing their current through the laptop's USB port.
- **Battery reading is taken before the switch**, so it reads the battery even with the
  switch OFF.
- **Charging:** the BQ24195 chip (`0x6B`) charges the battery from USB at up to ~500 mA.
- **`controller.begin()` configures that chip** (2 A USB input limit, charging on, chip
  watchdog off). The original `nanobot_arduino.ino` has `begin()` commented out, so the
  charger runs on its power-up defaults. `03_Hardened_Class_Sketch` restores it.
- **Buzzing with no battery / switch OFF (on USB):** likely the charger repeatedly
  detecting "no battery" while the motor boost converters, enabled from the same rail,
  start and stop. That's coil whine. It isn't harmful, but it's not a supported state.
  Plug in the battery and turn the switch ON.

## Tools

Upload with the Arduino IDE (board: Arduino Nano 33 IoT) and open Serial Monitor at 115200.

- **`01_Diagnostic`:** full bench test. Send any character to start. It checks the IMU, the D11
  handshake and firmware version, battery, M1–M4 (asks y/n whether each moved), servos and
  encoders, then prints a summary every 10 s. If no carrier is attached, every carrier test
  fails, which is expected.
- **`02_Flasher_Reflash_D11`:** reflashes the D11 (Arduino's own Flasher example). Needs the
  battery connected and switch ON. Success ends with `New version: 0...`. If it hangs, the D11
  isn't on the I2C bus at all: reseat the carrier, and if it still hangs it's a hardware fault.
- **`03_Hardened_Class_Sketch`:** the class sketch with WiFi removed (serial only, no
  WiFiNINA needed). A missing IMU or carrier no longer hangs the board. It blinks the onboard LED instead and keeps running:
  - **3 blinks, repeating:** IMU failed
  - **6 blinks, repeating:** carrier/D11 failed (M3/M4 still work)

  If a board still hangs with no blink pattern, the I2C lines are physically stuck (seating or
  wiring). Software can't fix that.
- **`04_I2C_Scan`:** prints the addresses on the I2C bus plus the D11 firmware version every
  second. It's the fastest carrier check and needs no motors (see the table above).
