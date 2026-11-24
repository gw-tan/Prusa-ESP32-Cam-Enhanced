# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build system

This project uses **PlatformIO**. Install it via the [VS Code extension](https://platformio.org/install/ide?install=vscode) or the [CLI](https://docs.platformio.org/en/latest/core/installation/index.html).

Board selection is done via named environments in `platformio.ini` — no manual header editing required.

```bash
pio run -e ai_thinker                    # compile for AI Thinker
pio run -e ai_thinker -t upload          # compile + flash
pio run -e ai_thinker -t erase && \
  pio run -e ai_thinker -t upload        # erase + flash (first flash on new board)
pio device monitor -b 115200             # serial monitor
pio run                                  # compiles ONLY ai_thinker (default_envs)
pio run -e ai_thinker -e wrover -e esp32s3_eye \
        -e freenove_s3_wroom -e xiao_esp32s3 -e esp32s3_cam   # every board
```

> **Windows note:** `pio` is not on PATH by default. Use the full path:
> `~/.platformio/penv/Scripts/pio.exe` or add it to PATH.

Available environments and their board targets:

| Environment | Board |
|---|---|
| `ai_thinker` | AI Thinker ESP32-CAM |
| `wrover` | Freenove ESP32-Wrover-Dev |
| `esp32s3_eye` | ESP32-S3-EYE 2.2 |
| `freenove_s3_wroom` | Freenove ESP32-S3-Wroom |
| `xiao_esp32s3` | Seeed XIAO ESP32-S3 Sense |
| `esp32s3_cam` | ESP32-S3-CAM |

All library dependencies are declared in `platformio.ini` and fetched automatically. The `.pio/` build directory is gitignored.

## Architecture

### Board abstraction layer

Each supported board has a `src/module_<BOARD_NAME>.h` file (e.g. `src/module_AI_Thinker_ESP32-CAM.h`) that defines all GPIO pin numbers, flash LED control method (PWM/digital/NeoPixel), PSRAM availability, and feature flags. These are guarded by the `#ifdef` matching the board define set via `platformio.ini` `build_flags`. `src/mcu_cfg.h` holds all tuneable constants: task intervals, WDT timeout, photo fragment size, EEPROM address map, Prusa Connect URL paths, OTA server, factory defaults.

**Board selection is by macro value, and `mcu_cfg.h` must not clobber it.**
`module_templates.h` picks the header with `#if (true == BOARD)`, not `#ifdef` —
every board macro is always *defined*, so `#ifdef` would match all of them. The
values come from the environment's `-DBOARD=true` in `platformio.ini`, and
`mcu_cfg.h` supplies `#ifndef`-guarded `false` defaults for the rest. It used to
define all seven unconditionally, which overwrote the `-D` and made **every
environment build AI Thinker firmware** — `pio run -e wrover` produced an image
whose only WROVER-ness was its filename, and nothing in a green build showed it.
Keep those defaults `#ifndef`-guarded, and never add a fallback board: a build
with no flag must fail the `!= 1` guard in `module_templates.h`. CI now greps each
artifact for its own `OTA_ASSET_NAME` and for every other board's, which is what
catches a regression here.

**`OTA_ASSET_NAME` belongs in the board header, never in `mcu_cfg.h`.** OTA matches
GitHub release assets by exact filename, and that match is the only thing stopping a
board from installing another board's image — wrong camera pins at best, and an
ESP32-S3 app on an ESP32 will not boot at all. It must equal
`firmware-<platformio env>.app.bin`; `release.yml` fails the build if a published
board's header disagrees, and `ota.cpp` `#error`s if a header omits it entirely. A
board with no published binary keeps its declared name and is simply told the asset
is missing, which is the correct outcome.

Boards differ in what they physically have, and the SPA has to know: `json_input`
carries `sd_hw` (from `ENABLE_SD_CARD`) so a board without an SD card hides the
timelapse UI instead of offering buttons that answer 503. That is distinct from
`sd_status`, which reports whether a card is *inserted*.

**`STATUS_LED_ENABLE` is honoured in `sys_led.cpp`, not at the call sites.** It was
dead for a long time — every board initialised and toggled its status LED whatever
the flag said. That only became a real bug on the Freenove WROVER v3, where the
onboard LED is GPIO 2 and GPIO 2 is also SD D0, so a 400 ms blink lands in the
middle of a card transfer. `sys_led`'s methods compile to nothing when the flag is
false; keep it that way rather than guarding the five callers. Before assuming any
board's pin map, check the vendor's own example sketches — upstream's headers
describe one board revision and Freenove changed the hardware under them.

### EEPROM address map — read before touching it

The map in `mcu_cfg.h` is a **chain**: every `_START` is computed as the previous
entry's `_START + _LENGTH`. Deleting or resizing an entry silently moves every
address below it, and an app-only upgrade (the normal path, and what OTA does) keeps
the old NVS contents — so a provisioned device reads its WiFi password, Connect token
or PrusaLink API key from the wrong offset and comes back broken.

**Only append.** To retire a setting, leave its bytes in place as a reserved slot —
`EEPROM_ADDR_RESERVED_2B_*` is one, left behind by the external temperature sensor.
Reuse reserved slots before growing the map.

### FreeRTOS task model

`setup()` creates **three** tasks (reduced from eight — the five housekeeping loops
were each a stack + TCB + watchdog subscription to do seconds of work per minute):

- `System_TaskCaptureAndSendPhoto` (1 s tick, priority 2, **Core 1**) — capture and
  upload. Pinned to Core 1 deliberately to keep TLS off the WiFi core.
- `System_TaskMain` (100 ms tick, priority 3, Core 0) — button polling only.
- `System_TaskHousekeeping` (100 ms tick, priority 1, Core 0) — WiFi management, WiFi
  watchdog, SD checks, serial config, telemetry, status LED, PrusaLink polling and OTA
  service, each on its own interval off one tick.

Anything doing HTTPS or ArduinoJson must run on Housekeeping (8192 B stack), never on
the AsyncTCP task. `loop()` only resets the WDT.

The task watchdog subscribes Core 0's idle task only; the stock all-cores mask added
reboot risk because Core 1 is where the upload task blocks inside mbedTLS.

### Camera capture flow (`camera.cpp`)

`CapturePhoto()` is the core method. It:
1. Guards against concurrent sends via `PhotoSending` flag
2. Takes `frameBufferSemaphore` (mutex) to own the frame buffer
3. Fires flash (PWM on GPIO4, 80% duty, configurable duration)
4. Takes one dummy frame to flush OV2640's sensor buffer
5. Attempts up to 5 real captures, validates byte-15 control flag == `0x00` and `len > 100`
6. Generates EXIF header and records `offset` into the JPEG for later splicing
7. Releases semaphore

When `StreamOnOff == true` (browser MJPEG stream active), `CapturePhoto()` takes a different path: it only sets `StreamSendingPhoto = true` and returns. The stream task (`CaptureStream()`) independently fills `FrameBufferDuplicate` (PSRAM-allocated copy) which is what the upload then reads.

**Camera orientation:** The AI Thinker OV2640 module is physically mounted upside down. Orientation is corrected via the EXIF orientation tag (`imageExifRotation`, stored in EEPROM). Photo viewers and modern video players (VLC 3+, Windows 11) respect this tag. Do **not** rely on sensor-level `vflip`/`hmirror` for orientation correction — use EXIF rotation instead.

### Prusa Connect upload (`connect.cpp`)

`SendDataToBackend()` opens a `WiFiClientSecure` TLS connection to `connect.prusa3d.com:443` using the embedded CA cert in `Certificate.h`. It sends an HTTP/1.1 PUT to `/c/snapshot` with `token` and `fingerprint` custom headers, streaming the JPEG body in 2048-byte chunks (`PHOTO_FRAGMENT_SIZE`). A separate PUT to `/c/info` sends a JSON device descriptor (board model, firmware version, resolution, WiFi MAC/IP).

Authentication credentials (`token`, `fingerprint`) are provisioned via the local web UI and persisted to EEPROM. The EEPROM layout is fully defined in `mcu_cfg.h` as a flat address map with named `_START` / `_LENGTH` pairs.

### Timelapse (`src/timelapse.cpp`, `src/timelapse.h`)

`TimelapseBuilder` writes MJPEG-in-AVI to SD card. Key facts:

- **Filename format:** `tl_<stamp>_<sanitised job name>_pNNN.avi`, e.g.
  `tl_20260803_074647_window_pane_holder_0.4n_0.25mm_P_p000.avi`. The `_pNNN` part
  suffix is required: timestamps have one-second resolution, so with segmentation on,
  two segments closing in the same second would collide and `SD_MMC.open(FILE_WRITE)`
  truncates an existing file.
- **Frame format:** Each AVI frame is written as `[EXIF header (FF D8 FF E1 … orientation tag)] + [JPEG image data from offset]` — the same structure as saved JPEG photos. This ensures correct orientation in video players. Do not strip the EXIF before calling `appendFrame`.
- **Frame writing:** `appendFrame(prefix, prefixLen, body, bodyLen)` — two-buffer overload avoids heap allocation per frame. The single-buffer variant `appendFrame(buf, len)` delegates to it.
- **Segmentation:** After `_maxSegFrames` frames (configurable, 0 = unlimited), the current AVI is finalised and a new one opened. Segment count persists across reboots in EEPROM.
- **Trigger modes:** `TL_TRIGGER_MANUAL` (0) = button/UI only; `TL_TRIGGER_PRUSA_LINK` (1) = auto-start/stop with PrusaLink print job.
- **Flash feedback:** `flashBlink()` pulses the camera flash LED — 2x150 ms on
  `begin()`, 3x100 ms on a successful `stop()`. It is the only feedback a button press
  gets, since the board has no other indicator, so keep it if you touch either path.
  Two details matter: the stop blink is inside `if (ok)`, which makes "no flash" a
  deliberate signal that finalising failed; and it saves/restores `GetFlashStatus()`
  so blinking does not clobber the user's LED Light setting. It uses `delay()` and is
  called *after* `xSemaphoreGive(_mutex)` — do not move it inside the lock.
  A deferred start (`begin()` without `force`, waiting on NTP/job name) returns `true`
  and does **not** blink; the blink comes later from `servicePendingStart()` when the
  file really opens. So the flash means "file open", not "request accepted".
- **`tl_trigger` JSON field:** Returned as **integer** (0 or 1) from `json_input`. UI comparisons must use `== 1` (loose equality), not `=== '1'` (strict string) — the server serialises it as a number.

### PrusaLink (`src/prusa_link.cpp`, `src/prusa_link.h`)

Polls the local PrusaLink API to detect print job state changes. When trigger mode is `TL_TRIGGER_PRUSA_LINK`, automatically calls `SystemTimelapse.begin()` when a print starts and `SystemTimelapse.stop()` when it ends. The status card (printer state, progress, temperatures) is polled every 5 s alongside `json_input`.

### Local web server (`src/WebServer.cpp`, `src/WebStream.cpp`)

`ESPAsyncWebServer` serves a config UI on port 80. Key routes:
- `GET /` — the SPA, gzipped
- `GET /stream.mjpg` — live MJPEG stream via `AsyncJpegStreamResponse` (sets `StreamOnOff = true` for the duration)
- `GET /saved-photo.jpg` — last captured still
- `GET /action_send` — sets `SendingIntervalExpired` flag to trigger an immediate upload cycle
- `GET /set_int`, `GET /set_bool`, `GET /set_token` — config setters that write to EEPROM
- `GET /set_tl_trigger?val=manual|printer` — sets timelapse trigger mode
- `GET /timelapse/start`, `/timelapse/stop`, `/timelapse/status`, `/timelapse/list`, `/timelapse/download`, `/timelapse/delete` — timelapse REST API
- `GET /ota/check`, `/ota/install`, `/ota/status` — on-demand OTA. Handlers only set a
  flag; the HTTPS work runs on Housekeeping. There is no polling.
- `GET /timelapse/download` — supports HTTP Range/206, implemented by hand

All web handlers run in the ESPAsyncTCP task context (still Core 0).

**Login (HTTP Basic auth)** is configured from the SPA's Security tab via
`/basicauth_cfg?auth_enable=&auth_username=[&auth_password=]`. Every route,
including `/stream.mjpg`, calls `Server_CheckBasicAuth()` — keep it that way for new
routes. The handler validates everything before applying anything (a half-applied
change can lock the user out), keeps the stored password when none is sent (the
password is never returned in `json_input`), and refuses to enable login with no
password. Usernames may not contain `:` (RFC 7617 splits on it).

**Local hostname**: the mDNS name (`/set_mdns`, WiFi tab) is also the DHCP hostname.
`WiFi.setHostname()` must run *before* `WiFi.mode()` in `WiFiMngt::Init()` or it
never reaches DHCP. Both firmware and SPA restrict it to one RFC 1123 label;
`DEVICE_HOSTNAME` is the fallback for an invalid stored value. Applies on reboot.

### Web UI — `webpage/index.html`

The web UI is a **single-page app** (`webpage/index.html`) — HTML + Tailwind + vanilla JS, all inline.

**After editing it, regenerate the compressed assets:**
```bash
cd webpage && python webpage_gz_generator.py
```
This writes `src/WebPage_gz.h` — `index.html` and `styles.css` gzipped as byte
arrays, served with `Content-Encoding: gzip` (~59 KB of flash saved). The generator
pins the gzip `mtime` to 0, so regenerating unchanged sources produces no diff.

**Tailwind is built ahead of time, not loaded from a CDN.** The CDN script made the
UI depend on internet access, and the device's own setup AP has none — the first-run
WiFi page rendered unstyled, as would any install on an isolated VLAN. Now:

```bash
cd webpage && npm ci && npm run build   # tailwind.src.css -> styles.css
```

Only needed when a class is added or removed. `styles.css` is committed; CI rebuilds
it and fails if the result differs.

The tradeoff: the CDN's JIT watched the live DOM and would generate any class the
moment it appeared. A prebuilt stylesheet only contains what Tailwind found as
**literal text** in `index.html`. `el.className = 'bg-' + colour` compiles and then
silently does nothing on the device. Every dynamic class in the SPA is a complete
literal today; `webpage/check_classes.py` verifies all 179 of them resolve, and runs
in CI. If you ever genuinely need a computed class, add it to `safelist` in
`tailwind.config.js`.

`src/WebPage.h` holds only the four small legal pages and the `MSG_*` defines.
They are served uncompressed and carry their own inline styling.

`webpage/index.html` is the whole SPA; its System/Connect/WiFi/TL-Files/Logs tabs are
inline in that file. **The upstream multi-page UI is gone** — `styles.css`,
`scripts.js`, the `page_*.html` files and the icon set were all unreachable from the
SPA and were removed, along with their routes and `WebPage_Icons.h` entries. The
only icon the firmware serves is the favicon; everything else is inline SVG in the
SPA. Do not add UI to a separate `page_*.html` expecting the SPA to show it.

**Color tokens** — Prusa Connect brand palette (dark mode):
| Token | Hex | Role |
|---|---|---|
| `orange` | `#FA6831` | Primary accent (real Prusa orange) |
| `base` | `#1e1e1e` | Page background |
| `surface` | `#2a2a2a` | Cards, nav |
| `panel` | `#333333` | Sidebar panels |
| `border` | `#444444` | All borders |

**Layout** — sidebar (drawer on mobile, fixed column on `lg+`) + main column:
- Camera viewport: `clamp(200px, 40vw, 58vh)`
- Mobile bottom action bar (fixed, `z-20`) — Stream toggle, Snap, Upload
- Toast container: `bottom-20` on mobile to clear the action bar

**JS patterns to follow:**
- All server-supplied strings must go through DOM node creation (`textContent`) or `escHtml()` — never raw `innerHTML` with untrusted data
- All mutating `fetch()` calls must have a `.catch()` handler that calls `toast('... failed', 'err')`
- Use `syncStream(bool)` to toggle streaming from any trigger — it syncs both the sidebar and mobile toggles
- `fetchStatus()` uses an `AbortController` (`S.statusController`) to cancel the previous in-flight request before issuing a new one — preserves this pattern
- `S._loaded` flag gates one-time seeding of form fields from `json_input`; do not add duplicate seeding elsewhere
- JSON fields from `json_input` may be integers or booleans — use loose equality (`== 1`, `== true`) not strict string comparison (`=== '1'`)

**Endpoints used by the SPA:**
- `json_input` — all status/config (polled every 5 s)
- `/timelapse/status` — recording state (polled every 5 s, same AbortController)
- `/saved-photo.jpg?<timestamp>` — snapshot refresh (cache-busted)
- `/stream.mjpg` — MJPEG stream src when streaming mode is active

