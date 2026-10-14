# Muse GenUI: five-tab MA35H0 HMI and NAU8822 audio DSP

This userspace example brings a white Muse interface, local media playback,
touch controls, a thermostat simulation and Nuvoton NAU8822 audio DSP controls
to an MA35 Linux HMI. It does not change the kernel build or drivers.

The native C++ service runs downloads, decoding, framebuffer rendering, touch,
EQ and headphone playback on the board. An Ethernet Internet connection
(including Internet Connection Sharing) provides routing only; a laptop media
renderer or decoder is not required. The native gateway also runs the Muse TLS/Noise session and GenUI API on the
board. The optional Python gateway remains available for initial BLE pairing
and the legacy voice-note workflow; see [GATEWAY.md](GATEWAY.md).

Hardware validation used a NuMaker-HMI-MA35H0 compact board with the vendor
Linux 5.10.140 BSP, 1024x600 BGRA framebuffer and single-contact resistive touch.
The header identifies MA35H0 and Winbond W29N08GV storage. No Linux 6.18 boot
or driver validation is claimed. Adapt framebuffer geometry, orientation,
input node and mixer routing for another MA35 board.

## What is included

- Muse chat, smaller text, automatic scrolling and Stop speech in the gateway.
- Native image/text/PDF/audio/video playback, windowed by default, with full
  screen exit, pan/scroll, explicit zoom, Replay and Stop controls.
- A ten-band CPU EQ with measured 4096-point FFT spectrum.
- A real five-band NAU8822 hardware EQ: 105, 300, 850, 2400 and 6900 Hz,
  independently adjustable from -12 to +12 dB in one-dB steps.
- Touch switches for CPU EQ, codec EQ, DAC limiter, microphone high-pass,
  microphone automatic gain and noise gate. Codec writes use ALSA controls
  with readback verification. EQ Off applies unity gain and retains settings
  in memory; it does not power down the codec EQ block.
- EQ Default resets both gain banks to flat, enables CPU EQ and sets hardware
  EQ to unity, leaving microphone/limiter switches unchanged.
- Relative "more bass", "less bass" and treble commands, and voice/music/flat
  presets exposed to Muse through the Gadget SDK command API.
- A simulated thermostat and real headphone volume/mute controls.

The spectrum measures PCM before codec processing, so codec EQ/limiter changes
are audible effects but are not represented in those bars. Microphone DSP
switches affect recording, not headphone playback. The codec supports 3D stereo
widening, but the validated BSP driver does not expose its depth control and
this demo does not enable it. See the [NAU8822 datasheet](https://www.nuvoton.com/export/resource-files/NAU8822DataSheetRev3.3.pdf).

## Build and install the board-native GenUI runtime

Build with an AArch64 Linux C/C++17 toolchain, pthreads, and OpenSSL headers and
libraries built for that same target toolchain. The portable Muse Noise core is
bundled with its license and pinned provenance under `native-media/vendor/noise_core`;
a separate ESP32 SDK checkout is not needed.

```sh
# Run from this directory. TLSLIB contains include/openssl and libssl/libcrypto
# archives at its root; use the target sysroot's libraries by omitting TLSLIB.
make CROSS_COMPILE=aarch64-linux-gnu- TLSLIB=/path/to/aarch64-openssl
# For the compact BSP static build that needs the IPv4 NSS workaround:
make CROSS_COMPILE=aarch64-linux-gnu- TLSLIB=/path/to/aarch64-openssl USE_ICS_DNS=1
# Build on an AArch64 board with development libraries installed:
make CROSS_COMPILE= LDFLAGS=
```

The build produces `muse-media`, `muse-gateway`, `quick-present` and
`youtube-resolve` under `native-media/build`, plus the optional legacy framebuffer
and touch helpers. `CC`, `CXX`, `CXXFLAGS` and `LDFLAGS` can be overridden.
Static networking libraries must match the selected libc and the board image;
the IPv4-only DNS shim avoids loading incompatible BSP NSS modules, but does
not make unrelated library ABIs compatible.

The board-native configuration uses the following SD layout:

| Board path | Dependency |
|---|---|
| `/mnt/sd/muse/media/` | The four built native executables |
| `/mnt/sd/muse/media/ffmpeg` | Board-compatible FFmpeg with video, audio and HTTPS decoding |
| `/mnt/sd/muse/media/curl` | Board-compatible curl with HTTPS |
| `/mnt/sd/muse/media/mutool` | MuPDF backend required to display generated PDFs |
| `/mnt/sd/muse/media/ca-certificates.crt` | Trusted CA bundle |
| `/mnt/sd/muse/media/font.ttf` | DejaVu Sans or another supported TrueType font |
| `/mnt/sd/muse/media/assets/` | Generated reports and optional header artwork |
| `/mnt/sd/muse/media-cache/` | Saved tab parameters, photos and decoded PDF/image rasters |
| `/mnt/sd/muse/state/` | Private identity, pairing, command schema and visual policy |
| `/mnt/sd/muse/tts/` | Optional speech script, speech engines, models and status |
| `/usr/bin/aplay`, `/usr/bin/amixer` | ALSA utilities |

The validated compact BSP uses Linux 5.10.140/glibc 2.31, FFmpeg 7.1.1,
curl 8.14.1 and MuPDF 1.26.1. Backends, models, compiled kernel modules and
third-party artwork are supplied separately. Review their licenses when
redistributing binaries. Artwork is optional: `muse-logo.png`, `nuvoton-logo.png`,
`winbond-logo.png`, `jollybot.png` and `athletics-logo.png` are loaded when present.

Mount an existing ext4 SD filesystem at `/mnt/sd` and provide an active
`mnt-sd.mount` unit. `native-media/mnt-sd.mount` is an example for `/dev/mmcblk1p1`;
adapt its device before installing it. The installer does not format storage,
create swap, or automatically install that mount definition.

Pair once using the [Muse Linux SDK](GATEWAY.md#setup), then explicitly migrate
its `identity.json`, `pairing.json` and optional `sdk_token` to the private SD
state directory. Native BLE onboarding is not implemented. The native gateway
reuses the SDK's credential format and then holds the TLS/Noise session,
registration, chat subscription and command execution on the board.

For deployment, install the host-side requirements, build the target executables,
and use a board account with device access and permission to install systemd units:

```sh
python3 -m venv venv
venv/bin/pip install -r requirements.txt
# Preview file paths first; this does not connect to the board.
venv/bin/python native-media/deploy_board_native.py --ssh-target root@BOARD --dry-run
# Existing paired board with backends already installed:
venv/bin/python native-media/deploy_board_native.py --ssh-target root@BOARD
# Initial deployment: supply backend files and explicitly selected SDK state.
venv/bin/python native-media/deploy_board_native.py --ssh-target root@BOARD \
  --backend-dir /path/to/board-backends --pairing-state /path/to/private-sdk-state
```

The installer stages all inputs before restarting `muse-media.service` and
`muse-gateway.service`, generates `commands.json` from the packaged media/GenUI
schemas, and preserves existing pairing unless `--pairing-state` is supplied.
Relay configurations can pass `--ssh-port` and `--ssh-host-alias`; normal SSH
key and host-key configuration apply. The API listens on board loopback port
8765 and the native local command socket is `/run/musegadget.sock` (mode 0600).
Stop the optional laptop gateway when migrating so both do not run the same
pairing; its `state/board-native.enabled` marker also makes that gateway exit.

`--enable-swap` installs the swap service only when a preinitialized
`/mnt/sd/muse/swapfile` exists. Swap increases capacity and remains much slower
than RAM. The renderer and gateway units cap memory at 38 MiB and 52 MiB.
The dedicated development-board units run as root for framebuffer, input and
ALSA access; adapt permissions for your device image.

Text **Read to me** uses eSpeak NG at `tts/piper/espeak-ng` with its voice data
under `tts/piper/espeak-ng-data`. The `speak` helper requires BusyBox ash with
`pipefail`. Automatic speech can use Piper at `tts/piper/piper`, with
`tts/models/en_US-lessac-low.onnx` and its `.onnx.json`; Kokoro selection is
available for separately installed assets and remains unverified for synthesis
on this board. Speech is optional for the five tabs. Stop voice cancels the
speech process group; radio/video playback takes ownership of headphone output.

## GenUI API and five tabs

The [GenUI SDK](genui/README.md) layers `Client`, `Executor`, command registration
and a local Unix transport on the Meta Muse Gadgets Linux SDK. A version-1
presentation contains source-labelled text/metrics/charts/tables, direct photo
URLs and a YouTube/direct-video URL. The board produces its PDF and updates
Text, Images, PDF and Video together. Audio preserves its radio station.

```sh
/mnt/sd/muse/gateway/muse-gateway --local '{"message":"show me stuff on Texas","genui":true}'
/mnt/sd/muse/gateway/muse-gateway --local '{"action":"genui.status"}'
/mnt/sd/muse/gateway/muse-gateway --local '{"action":"media.switch","params":{"kind":"pdf"}}'
```

| Tab | Board behavior |
|---|---|
| Text | Source-labelled document, metric cards, charts, tables, scroll and read/stop voice |
| Images | Up to 20 direct-photo slides; discovery selects up to 10; one-second default interval |
| PDF | Native PDF 1.4 generated from the document, with values, pagination and source links |
| Video | Public YouTube/direct streams, low-resolution CPU playback and headphone audio |
| Audio | Independent Internet radio presets, CPU EQ, NAU8822 hardware EQ and live spectrum |

Tab parameters and decoded image/PDF caches persist on SD. Single-contact
resistive touch has enlarged tab targets and constrained panning; double-tap
zoom is disabled. Explicit zoom controls remain available.

Visual prompts start Wikipedia overview/media-list and YouTube discovery in
parallel on the board. This overview is labelled as Wikipedia, including article
freshness; it is not a live news or sports feed. Standalone `genui.present`
clients may supply other retrieved sources. A dismissible popup reports actual
Text/Images/PDF/Video preparation, API activity and errors. Download retries for
HTTP 429/transient failures affect only the failed item. Preparation has a
60-second deadline; 30 seconds is the performance target, not a guarantee.

The first board-managed presentation stays until a new prompt. Late Muse visual
updates and assistant text do not replace it; the policy survives gateway
restart. Local SDK clients can still submit explicit presentations. Radio
selection stays independent. Failed or unavailable sources remain visible as
errors/unavailable rather than being counted as ready.

On the MA35H0 test board, a fresh Beijing request on 2026-10-09 reached Text in
1.04 s, PDF in 2.58 s, ten decoded photos in 7.15 s, and a verified first YouTube
frame in 10.93 s. Source availability and network conditions can change timings.
PDF fonts currently normalize non-ASCII text. YouTube videos needing sign-in,
DRM or player challenges may be unavailable.

The existing framebuffer/touch/NAU8822 interfaces are used directly. The optional
[DMA geometry patch](native-media/patches/0001-ma35h0-dma-reject-partial-cyclic-periods.patch)
is supplied for BSP integration but has not been installed on the board.
Runtime PCM uses whole ALSA periods to avoid the reported BSP descriptor overrun.
VC8000 hardware decoding remains disabled after board hangs during testing;
this PR uses CPU rendering and decoding.

## Five-band hardware "more bass" example

From the board, POST these JSON bodies to `http://127.0.0.1:8765/media`:

```json
{"action":"media.show","params":{"kind":"video","url":"/opt/muse-media/assets/demo.mp4","caption":"Muse audio DSP demo","fullscreen":false}}
{"action":"media.control","params":{"operation":"dsp","feature":"software_eq","enabled":false}}
{"action":"media.control","params":{"operation":"eq_view","value":2}}
{"action":"audio.tune","params":{"preset":"bass_up","engine":"hardware","amount_db":3}}
{"action":"media.status"}
```

Each hardware `bass_up` raises the 105/300-Hz bands by the requested amount,
clamped to +12 dB, and leaves the other three bands unchanged. Calling it twice
from flat produces `[6,6,0,0,0]`. Selecting the hardware engine enables codec EQ;
CPU EQ is disabled explicitly above so this example isolates codec processing.

```json
{"action":"media.control","params":{"operation":"dsp","feature":"limiter","enabled":true}}
{"action":"media.control","params":{"operation":"eq_default"}}
```

For Muse voice/chat integration, the optional gateway advertises `audio.tune`
and `media.control` with natural-language descriptions. A spoken "I want more
bass" can invoke `bass_up`; `engine: active` selects the hardware EQ only when
it is enabled and CPU EQ is disabled. Registration requires the gateway to run
and reconnect. The native API alone does not perform speech recognition.
See [GATEWAY.md](GATEWAY.md) for BLE, voice-note, TTS, temperature and headphone
control examples. Its dependencies install the separate Muse Gadget SDK from
a pinned public source commit.

## Limits and validation

Video currently uses CPU decoding and rendering, targeting 480x270 at 20 fps;
the tested effective GUI rate was approximately 15 fps. VC8000 playback is
quarantined following a decoder-only kernel hang. GC520 acceleration is not
implemented by this renderer. Driver/module presence is not usable acceleration.
The single-contact panel cannot perform true two-finger pinch.

PCM is stereo S16_LE, 48 kHz, using `hw:0,0`, 8192-frame buffers and 1024-frame
periods. The validated 5.10 DMA implementation mishandles fractional cyclic
periods; time-based ALSA negotiation caused a whole-board hang. The fixed eight
whole-period geometry avoids that case. The historical guard patch under
`native-media/patches/` is reference material for the 5.10 BSP, is not applied
by this example and must not be assumed valid against another kernel version.

Hardware checks verified real ALSA EQ register changes (0 -> +3 -> +6 dB bass),
DSP switch readback, physical touch reset/Replay, spectrum updates, full-clip
playback with no reported ALSA underruns, and a responsive UI after Stop.
Volume uses discrete codec steps. Playback smoothness depends on input codec,
resolution, bitrate, available memory and the installed FFmpeg build.

Host tests need g++ and pytest; optional gateway tests also need its Python
requirements:

```sh
python3 -m pytest -q native-media
python3 -m pytest -q .
# OpenSSL backend known-answer/authentication checks on the host:
make check-crypto CROSS_COMPILE= LDFLAGS=
```

The native tests exercise actual C++ parsing, tone adjustments, limits, codec
register encoding, filter transfer gains, stereo isolation and FFT spectrum.
See [native-media/README.md](native-media/README.md) for the complete media API.

Source is Apache-2.0 unless otherwise indicated. Vendored nlohmann JSON is MIT;
stb is dual MIT/public-domain. Their notices are retained in each header.
Pairing credentials, recordings, generated speech, models, logs, binaries and
runtime state are ignored by Git.
