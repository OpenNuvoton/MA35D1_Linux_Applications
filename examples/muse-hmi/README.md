# Muse HMI and NAU8822 audio DSP demo

This userspace example brings a white Muse interface, local media playback,
touch controls, a thermostat simulation and Nuvoton NAU8822 audio DSP controls
to an MA35 Linux HMI. It does not change the kernel build or drivers.

The native C++ service runs downloads, decoding, framebuffer rendering, touch,
EQ and headphone playback on the board. An Ethernet Internet connection
(including Internet Connection Sharing) provides routing only; a laptop media
renderer or decoder is not required. The optional Python gateway supplies Muse
BLE pairing, chat streaming, voice notes and Piper TTS. That gateway remains a
separate runtime requirement for those Muse features.

Hardware validation used a NuMaker-HMI-MA35H0 compact board with the vendor
Linux 5.10.140 BSP, 1024x600 BGRA framebuffer and single-contact resistive touch.
The on-screen MA35D1 branding refers to the MA35 family demo. No Linux 6.18 boot
or driver validation is claimed. Adapt framebuffer geometry, orientation,
input node and mixer routing for another MA35 board.

## What is included

- Muse chat, smaller text, automatic scrolling and Stop speech in the gateway.
- Native image/text/PDF/audio/video playback, windowed by default, with full
  screen exit, pan/scroll, double-tap zoom, Replay and Stop controls.
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

## Build and install the board-native demo

Use an aarch64 Linux C/C++ toolchain with C++17, pthreads and static runtime
libraries. On a host with the usual GNU cross compiler:

```sh
make -C examples/muse-hmi
# Or, from this directory, use a BSP compiler prefix:
make CROSS_COMPILE=aarch64-poky-linux-
```

Use `CROSS_COMPILE=` for a native aarch64 build. Override `CC`, `CXX` or
`LDFLAGS` for a different toolchain or dynamically linked board image.

Copy `native-media/build/muse-media` and optional `youtube-resolve` to
`/opt/muse-media/`. The service also expects:

| Board path | Dependency |
|---|---|
| `/opt/muse-media/ffmpeg` | Board-compatible FFmpeg with the desired decoders and raw RGB/PCM output |
| `/opt/muse-media/curl` | Board-compatible curl with HTTPS |
| `/opt/muse-media/mutool` | Optional MuPDF renderer for PDF/SVG |
| `/opt/muse-media/ca-certificates.crt` | Current trusted CA bundle |
| `/opt/muse-media/font.ttf` | DejaVu Sans or another supported TrueType font |
| `/opt/muse-media/assets/` | Local media and optional `muse-logo.png` / `jollybot.png` artwork |
| `/usr/bin/aplay`, `/usr/bin/amixer` | ALSA utilities |

Existing distro executables can be symlinked at the specified paths. The compact
BSP used separately built FFmpeg 7.1.1, curl 8.14.1 and MuPDF 1.26.1. Those
executables, voice models, copyrighted media and artwork are not bundled.
Supply only assets you are permitted to distribute. Refer to each backend's
license before redistributing its binaries.

Install `native-media/muse-media.service` under `/etc/systemd/system/`, then
run `systemctl daemon-reload` and `systemctl enable --now muse-media.service`.
It starts idle and does not auto-play downloaded media. It needs permissions
for `/dev/fb0`, `/dev/input/event0`, ALSA PCM/mixer access and backend scheduling;
the example unit currently runs as root on a dedicated development board.
The API binds board loopback port 8765.

For compact images using glibc 2.31, networking backends must match that ABI.
`native-media/ics_dns.c` documents the IPv4 resolver shim used to avoid loading
incompatible NSS modules into newer static binaries. It is an optional backend
build component, not part of the Muse renderer or a general IPv6 resolver.

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
```

The native tests exercise actual C++ parsing, tone adjustments, limits, codec
register encoding, filter transfer gains, stereo isolation and FFT spectrum.
See [native-media/README.md](native-media/README.md) for the complete media API.

Source is Apache-2.0 unless otherwise indicated. Vendored nlohmann JSON is MIT;
stb is dual MIT/public-domain. Their notices are retained in each header.
Pairing credentials, recordings, generated speech, models, logs, binaries and
runtime state are ignored by Git.
