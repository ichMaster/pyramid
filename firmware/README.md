# Pyramid firmware

AtomS3R + Echo Base firmware for the Pyramid voice AI assistant. The device is
**thin** — I/O and a status screen only; no persona/LLM/memory logic lives here
(ARCHITECTURE §Components). Built with **PlatformIO** (from v1.1; v0 used the
Arduino IDE — the sketch `pyramid/pyramid.ino` moved to `src/main.cpp`).

## Layout

```
firmware/
  platformio.ini        # PlatformIO project: atoms3r (device) + native (host tests)
  src/
    main.cpp            # the firmware: board + Wi-Fi + serial + LLM glue (was pyramid.ino)
    line_reader.h       # pure: non-blocking line reader (host-testable)
    serial_protocol.h   # pure: text_in parse (host-testable)
    chat_api.h          # pure: LLM request build / reply parse / retry class (host-testable)
    history.h           # pure: short rolling conversation history (host-testable)
    backoff.h           # pure: capped exponential backoff (host-testable)
    sse.h               # pure: chunked + SSE streaming decode (host-testable)
    config.example.h    # config template — copy to config.h (gitignored)
  test/
    test_line_reader.cpp  # host unit test for the serial logic
    test_chat_api.cpp     # host unit test for the LLM JSON build/parse + retry
    test_history.cpp      # host unit test for history windowing
    test_backoff.cpp      # host unit test for the backoff schedule
    test_sse.cpp          # host unit test for the streaming decode pipeline
```

`src/` holds the firmware and the pure (Arduino-free) headers it shares with the
host tests. The AtomS3R has no built-in PlatformIO board definition, so the env
targets the ESP32-S3 (`esp32-s3-devkitc-1`) with the AtomS3R's USB flags
(`ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`); M5Unified detects the actual
board at runtime.

## What it does so far

- **v0.1 (PYR-001):** board + Wi-Fi + USB-CDC line channel; a typed line
  becomes a `text_in` event. Status logs gated by `DEBUG_SERIAL`.
- **v0.2 (PYR-002):** each `text_in` is sent to **Claude** via the Anthropic
  Messages API over **direct HTTPS** (persona as the top-level `system`), and
  the model's reply is **streamed** to serial token by token (SSE,
  `stream:true`). HTTP/JSON errors surface as one `error: <msg>` line and the
  loop never hangs. Replies are in Ukrainian per the persona.
- **v0.3 (PYR-003):** robustness. A short **rolling history** (`HISTORY_MAX_TURNS`)
  is resent with each request for context; the LLM call does a **bounded retry**
  with exponential backoff on transient failures (transport / 429 / 5xx) and
  surfaces a clear line otherwise; **Wi-Fi loss auto-recovers** with non-blocking
  exponential backoff, input is paused while offline, and the LCD shows
  `idle / thinking / error / offline`. Only successful turns are committed to
  history, so a failed call can't poison the context.

After each reply the device prints a per-turn stats line, e.g.
`[stats] first_token=420 ms  total=1180 ms  tokens: in=48 out=73 total=121`.
Because the reply is streamed, `first_token` is the genuine time to the first
streamed token (well below `total`). `total` is the whole turn including any
retries, and the token counts come from the API's `usage` (input from
`message_start`, output from the final `message_delta`).

The persona, model, endpoint, and API key all live in `config.h` — behavior is
config-driven, not hardcoded. Default model is `claude-haiku-4-5-20251001`
(fast/cheap for short replies); swap to Sonnet/Opus in config for more depth.
**Security:** the key is extractable from a flashed device; acceptable only
under the private allowlist model (ARCHITECTURE §Security) — never publish such
firmware. TLS uses `setInsecure()` in v0 (no cert pinning).

## Build & flash (PlatformIO)

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/) (`pio`),
then from `firmware/`:

1. `cp src/config.example.h src/config.h` and set `WIFI_SSID` / `WIFI_PASS`,
   plus the Anthropic settings `LLM_ENDPOINT` / `LLM_MODEL` / `LLM_API_KEY`
   (an `sk-ant-…` key) / `LLM_ANTHROPIC_VERSION` / `LLM_MAX_TOKENS` /
   `LLM_PERSONA` / `HISTORY_MAX_TURNS` (and `DEBUG_SERIAL`). `config.h` is
   gitignored.
2. `pio run` — compile (first build fetches M5Unified + ArduinoJson).
3. `pio run -t upload` — flash the AtomS3R (auto-detects the port, or
   `--upload-port /dev/cu.usbmodemXXXX`).
4. `pio device monitor` — serial @115200; type a line, press Enter, the device
   replies via the LLM. Wi-Fi state is logged on boot.

> v0 was built in the Arduino IDE (board **M5AtomS3R**) / `arduino-cli compile
> --fqbn m5stack:esp32:m5stack_atoms3r`. That path still works against `src/`
> if needed, but PlatformIO is the supported toolchain from v1.

## Host tests (pure logic)

The line reader, `text_in` parser, history, backoff, the streaming decode
(chunked + SSE + event parsing), and LLM JSON build/parse are pure C++, so they
run on the host (formally folded into PlatformIO's native test env in v1).
`test_chat_api` and `test_sse` need ArduinoJson's headers on the include path —
point `-I` at your installed library's `src/` (e.g.
`~/Documents/Arduino/libraries/ArduinoJson/src`):

```sh
cd test
AJ="$HOME/Documents/Arduino/libraries/ArduinoJson/src"
c++ -std=c++17 -I../src test_line_reader.cpp -o test_line_reader && ./test_line_reader
c++ -std=c++17 -I../src test_history.cpp     -o test_history     && ./test_history
c++ -std=c++17 -I../src test_backoff.cpp     -o test_backoff     && ./test_backoff
c++ -std=c++17 -I../src -I"$AJ" test_chat_api.cpp -o test_chat_api && ./test_chat_api
c++ -std=c++17 -I../src -I"$AJ" test_sse.cpp      -o test_sse      && ./test_sse
```

The on-device read path (TLS socket + streamed SSE) only runs on the board, so
it is covered by compile + the manual DoD check; the decode logic above is
fully host-tested (including a `data:` line split across a chunk boundary).

On-device behavior (Wi-Fi join, LCD, button) and a live LLM round-trip — the
HTTPS call succeeding and the reply actually being in Ukrainian — are verified
by the manual DoD checks in ROADMAP §v0.1–v0.2, since they need the board, a
real API key, and the network.
