# Pyramid firmware

AtomS3R + Echo Base firmware for the Pyramid voice AI assistant. The device is
**thin** — I/O and a status screen only; no persona/LLM/memory logic lives here
(ARCHITECTURE §Components). In **v0** the firmware is built in the **Arduino
IDE** with M5Unified; it migrates to PlatformIO in v1.

## Layout

```
firmware/
  pyramid/
    pyramid.ino         # the sketch: board + Wi-Fi + serial glue
    line_reader.h       # pure: non-blocking line reader (host-testable)
    serial_protocol.h   # pure: text_in parse + reply formatting (host-testable)
    config.example.h    # config template — copy to config.h (gitignored)
  test/
    test_line_reader.cpp  # host unit test for the pure logic
```

`pyramid/` is the Arduino sketch folder (the `.ino` matches the folder name);
the extra `.h` files compile alongside it. `test/` is outside the sketch folder
so the Arduino build ignores it.

## v0.1 — what it does (PYR-001)

Boots the board, connects Wi-Fi from `config.h`, opens USB-CDC serial at
**115200**, and turns it into a line channel: a typed line becomes a `text_in`
event and is echoed back (`echo: <text>`). Status logs route through `logf()`,
gated by `DEBUG_SERIAL`. No AI yet — the LLM reply lands in v0.2 (PYR-002).

## Build & flash (Arduino IDE)

1. Install the **M5Stack** board package and the **M5Unified** library.
2. Select board **M5AtomS3R**.
3. `cp pyramid/config.example.h pyramid/config.h` and set `WIFI_SSID` /
   `WIFI_PASS` (and `DEBUG_SERIAL`).
4. Open `pyramid/pyramid.ino`, compile, and upload.
5. Open the Serial Monitor at **115200**, type a line, press Enter — the device
   echoes it back. Wi-Fi state is logged on boot.

### Build from the CLI (optional)

```sh
cp pyramid/config.example.h pyramid/config.h   # then fill in creds
arduino-cli compile --fqbn m5stack:esp32:m5stack_atoms3r pyramid
```

## Host tests (pure logic)

The line reader and `text_in` parser are pure C++ with no Arduino dependency,
so they run on the host (formally folded into PlatformIO's native test env in
v1):

```sh
cd test
c++ -std=c++17 -I../pyramid test_line_reader.cpp -o test_line_reader && ./test_line_reader
```

On-device behavior (Wi-Fi, LCD, button) is verified by the manual DoD check in
ROADMAP §v0.1, since it needs the board.
