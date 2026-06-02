# Pyramid firmware

AtomS3R + Echo Base firmware for the Pyramid voice AI assistant. The device is
**thin** — I/O and a status screen only; no persona/LLM/memory logic lives here
(ARCHITECTURE §Components). In **v0** the firmware is built in the **Arduino
IDE** with M5Unified; it migrates to PlatformIO in v1.

## Layout

```
firmware/
  pyramid/
    pyramid.ino         # the sketch: board + Wi-Fi + serial + LLM glue
    line_reader.h       # pure: non-blocking line reader (host-testable)
    serial_protocol.h   # pure: text_in parse (host-testable)
    chat_api.h          # pure: LLM request build / reply parse (host-testable)
    config.example.h    # config template — copy to config.h (gitignored)
  test/
    test_line_reader.cpp  # host unit test for the serial logic
    test_chat_api.cpp     # host unit test for the LLM JSON build/parse
```

`pyramid/` is the Arduino sketch folder (the `.ino` matches the folder name);
the extra `.h` files compile alongside it. `test/` is outside the sketch folder
so the Arduino build ignores it.

## What it does so far

- **v0.1 (PYR-001):** board + Wi-Fi + USB-CDC line channel; a typed line
  becomes a `text_in` event. Status logs gated by `DEBUG_SERIAL`.
- **v0.2 (PYR-002):** each `text_in` is sent to a cloud LLM over **direct
  HTTPS** (persona system prompt from config, single user turn), and the
  model's reply is printed to serial. The call is synchronous (`thinking…`
  while waiting); HTTP/JSON errors surface as one `error: <msg>` line and the
  loop never hangs. Replies are in Ukrainian per the persona. Rolling history
  + Wi-Fi auto-reconnect land in v0.3 (PYR-003).

The persona, model, endpoint, and API key all live in `config.h` — behavior is
config-driven, not hardcoded. **Security:** the key is extractable from a
flashed device; acceptable only under the private allowlist model (ARCHITECTURE
§Security) — never publish such firmware. TLS uses `setInsecure()` in v0
(no cert pinning).

## Build & flash (Arduino IDE)

1. Install the **M5Stack** board package and the **M5Unified** + **ArduinoJson**
   (v7) libraries.
2. Select board **M5AtomS3R**.
3. `cp pyramid/config.example.h pyramid/config.h` and set `WIFI_SSID` /
   `WIFI_PASS`, plus `LLM_ENDPOINT` / `LLM_MODEL` / `LLM_API_KEY` /
   `LLM_PERSONA` (and `DEBUG_SERIAL`).
4. Open `pyramid/pyramid.ino`, compile, and upload.
5. Open the Serial Monitor at **115200**, type a line, press Enter — the device
   replies via the LLM. Wi-Fi state is logged on boot.

### Build from the CLI (optional)

```sh
cp pyramid/config.example.h pyramid/config.h   # then fill in creds
arduino-cli compile --fqbn m5stack:esp32:m5stack_atoms3r pyramid
```

## Host tests (pure logic)

The line reader, `text_in` parser, and LLM JSON build/parse are pure C++, so
they run on the host (formally folded into PlatformIO's native test env in v1).
`test_chat_api` needs ArduinoJson's headers on the include path — point `-I` at
your installed library's `src/` (e.g.
`~/Documents/Arduino/libraries/ArduinoJson/src`):

```sh
cd test
c++ -std=c++17 -I../pyramid test_line_reader.cpp -o test_line_reader && ./test_line_reader
c++ -std=c++17 -I../pyramid -I"$HOME/Documents/Arduino/libraries/ArduinoJson/src" \
  test_chat_api.cpp -o test_chat_api && ./test_chat_api
```

On-device behavior (Wi-Fi join, LCD, button) and a live LLM round-trip — the
HTTPS call succeeding and the reply actually being in Ukrainian — are verified
by the manual DoD checks in ROADMAP §v0.1–v0.2, since they need the board, a
real API key, and the network.
