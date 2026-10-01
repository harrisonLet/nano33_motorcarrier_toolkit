# Nano 33 IoT + Motor Carrier Troubleshooting Toolkit

Built for ECE 3610. Three tools, in the order you should reach for them.

## Background, in short

- **Nano 33 IoT** uses *native USB* (the SAMD21 chip itself is the USB device). There's
  no separate USB-serial chip, so the serial port only exists while the firmware is
  alive and not stuck. If the chip resets, browns out, or hangs, the port can vanish
  and reappear -- that's not a driver bug, that's the chip blinking out.
- **The Motor Carrier is not a dumb breakout.** It has its own second microcontroller
  on it (the "D11", an ATSAMD11) running its own firmware, separate from whatever's on
  the Nano. The Nano talks to it over **I2C at address 0x66** using a small custom
  command protocol.
- **Not all 4 motor ports are the same.** `M1`/`M2` and **all 4 servo headers** go
  *through* the D11 over I2C. `M3`/`M4` are wired *directly* to GPIO pins on the Nano
  itself -- no I2C, no D11, nothing to do with the carrier's co-processor at all. This
  is the single most useful fact for diagnosing "some ports work, some don't": if
  M3/M4 work but M1/M2 and servos don't, the Nano is fine and the D11 is the problem.
- **The D11 can get stuck in its own bootloader** (not running motor-control firmware)
  and need re-flashing. Arduino's own stock examples check for this explicitly and
  print "Is the red LED blinking? You may need to update the firmware with the
  Flasher sketch" -- but the class's `nanobot_arduino.ino` has that check commented
  out, so a dead D11 currently fails silently instead of telling anyone.
- **Students never open the Serial Monitor** -- they only run MATLAB. So any
  diagnostic message the firmware prints during boot is invisible in normal use.
  That's why these tools lean on LED blink patterns and a from-the-bench test sketch
  instead of assuming anyone is watching Serial output.

## 1. `01_Diagnostic/` -- run this first on any suspect board

A standalone sketch, no MATLAB involved. Flash it, open the Arduino Serial Monitor
at 115200 baud, and it runs through every subsystem in isolation: onboard IMU (I2C
baseline), the D11 handshake + firmware version, battery sensing, M1-M4 (spins each
one, asks you to confirm by eye), servo1-4 (sweeps each, asks you to confirm), and
both encoders. It never stops early -- if one test fails, the rest still run, so you
get a full picture instead of one cryptic error.

It ends with a plain-English verdict, e.g.:
- IMU fails -> suspect the Nano board itself.
- IMU + M3/M4 pass but the D11 handshake fails -> go re-flash the D11 (step 2).
- Nothing on the carrier works, not even M3/M4 -> check seating/power before
  assuming it's a firmware problem.
- One of M1/M2 fails but not the other, and the D11 handshake passed -> that's a
  mechanical/solder fault on that one channel, not firmware.

**To use:** open `01_Diagnostic.ino` in the Arduino IDE (needs the `ArduinoMotorCarrier`
and `Arduino_LSM6DS3` libraries installed -- same ones already in
`nanobot_edu/libraries/`), select "Arduino Nano 33 IoT" as the board, upload, open
Serial Monitor.

## 2. `02_Flasher_Reflash_D11/` -- re-flashes the D11 co-processor

This is the library's own standalone firmware updater (copied unmodified from
`ArduinoMotorCarrier/examples/Flasher/`). It writes known-good firmware to the D11
over I2C, byte by byte, with a CRC check per block. Use it when the diagnostic sketch
(or the stock library examples) report the D11 as unresponsive / its red LED is
blinking.

**To use:**
1. Plug in the Nano 33 IoT with the carrier attached, battery connected, carrier
   switch ON.
2. Open `02_Flasher_Reflash_D11.ino` in the Arduino IDE, select "Arduino Nano 33 IoT",
   upload it.
3. Open the Serial Monitor (115200 baud). You'll see: `Reset D11`, `Erase flash`,
   then a stream of addresses/CRCs as it writes ~every 64-byte block, then
   `Booting FW` and finally `New version: ...` with a real version string (not
   starting with `0`).
4. If it hangs or the version still comes back starting with `0`, that board's D11
   is not responding to I2C at all -- that's a hardware fault (bad connector seating,
   bad solder joint, or a dead D11 chip), not something a re-flash can fix. Try
   reseating the carrier on the header pins first.
5. Once it reports a real version, re-flash the board with your normal class sketch
   (or the hardened one in step 3) and re-run the diagnostic sketch to confirm.

The firmware image itself is baked into `fw_nano.h` / `fw_mkr.h` as byte arrays --
you don't need the D11 firmware source to do this, it's just data this sketch pushes
over the wire.

## 3. `03_Hardened_Class_Sketch/` -- patched version of `nanobot_arduino.ino`

Same sketch the class already uses, with one behavioral change: the carrier
handshake (`controller.begin()`) that the library's own examples do is restored, and
made **non-fatal** instead of an infinite `while(1)` hang. Same treatment for the
IMU check. On failure, instead of locking up silently, the board blinks its onboard
LED in a pattern you can read without a laptop:

- **3 blinks, repeating** -> IMU didn't respond.
- **6 blinks, repeating** -> Motor Carrier / D11 didn't respond. M1/M2/servos/
  encoders won't work; M3/M4 still will, since they don't touch the D11.

This should cut down on the "board is just dead, no idea why" reports at the bench --
a TA can glance at the LED and immediately know which subsystem to chase, and a dead
carrier no longer takes the whole board down with it (M3/M4 and everything else
keeps working).

**Known limitation:** this fixes the common case, where the D11 is unresponsive and
cleanly NACKs/returns an error. If the I2C bus itself is electrically stuck (e.g. SDA
held low by a bad connection), `Wire`'s calls can still block -- no software timeout
exists in this library for that case. If you restore this sketch and a board *still*
hangs with no LED pattern at all, that's a real physical fault on the I2C lines
(seating/wiring), not something this patch can paper over -- treat it as a hardware
issue, not a re-flash issue.

To deploy: open `03_Hardened_Class_Sketch.ino` in the Arduino IDE alongside its
`udp_access_point.h/.cpp` and `credentials.h` (same files the original sketch uses),
select "Arduino Nano 33 IoT", upload as normal.

## A worth-checking MATLAB-side wrinkle (not yet fixed here)

`nanobot.m`'s constructor calls `fopen(obj.arduino)` right after creating
`obj.arduino = serialport(...)`. Modern MATLAB's `serialport` object opens the
connection at creation time and generally doesn't need/support `fopen` (that's a
leftover from the older `serial()` interface). It's worth confirming on a lab
machine whether that line errors, is silently ignored, or does something you don't
expect -- and whether stale `nanobot`/`serialport` objects left over in a student's
workspace from a previous crashed run (not `clear`ed between attempts) are holding a
port open and causing the "failed to open"/"specified amount of data..." errors on
the *next* attempt. Didn't want to change the class's MATLAB code without walking
through it with you first, but it's a likely contributor to the "sometimes it just
won't connect" reports and an easy thing to check next.
