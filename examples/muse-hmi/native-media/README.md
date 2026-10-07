# MA35 board-native media API

The API, HTTPS downloads, decoding, framebuffer rendering, touch gestures and
headphone playback run on the MA35 Linux board. Ethernet ICS supplies Internet
routing only. No laptop renderer, Python process or media proxy is required.
The existing BLE/Muse chat gateway is a separate component; this service does
not by itself move that pairing/chat implementation onto the board.

POST JSON to `http://127.0.0.1:8765/media` from a board-local client. The API
binds loopback and requires no phone confirmation. Muse's own upload/device
approval prompts belong to Muse and are not disabled by this service.

```json
{"action":"media.show","params":{"kind":"youtube","url":"https://youtu.be/MT-4Bk1Lw8g","caption":"YouTube","fullscreen":false}}
{"action":"media.show","params":{"kind":"pdf","url":"/opt/muse-media/assets/FSA506.pdf","page":1}}
{"action":"media.show","params":{"kind":"text","text":"Hello from the board"}}
{"action":"media.control","params":{"operation":"fullscreen","enabled":true}}
{"action":"media.control","params":{"operation":"zoom","value":2}}
{"action":"media.control","params":{"operation":"pan","x":-120,"y":-240}}
{"action":"media.control","params":{"operation":"page","value":2}}
{"action":"media.control","params":{"operation":"pause"}}
{"action":"media.control","params":{"operation":"resume"}}
{"action":"media.control","params":{"operation":"fit"}}
{"action":"media.control","params":{"operation":"stop"}}
{"action":"media.status"}
{"action":"media.capabilities"}
```

`media.show` acknowledges acceptance; poll `media.status` for loading, playing,
ready, ended or error and the rendered frame count. An accepted request is not
proof that playback started. A new request stops the previous process group.

Images: PNG/JPEG/WebP/BMP/TGA/PSD/HDR/PNM. Text and text-like Markdown/JSON/CSV:
plain text, not a browser. PDF/SVG pages use MuPDF. Animation/audio/video use
a minimal native FFmpeg build. Codec/container support depends on that build;
capabilities distinguishes installed backends. HLS is supported; DASH was not
included because this build lacks libxml2. Office files, interactive webpages,
DRM and arbitrary unknown formats are not universally supported.

Downloads for documents/images are limited to 8 MiB; video/audio are streamed.
Video output targets 480x270 at up to 20 fps with one decode thread. Input resolution,
codec and bitrate can still exceed the board's CPU/RAM budget: choose low
resolution streams. This is not a promise of smooth 1080p playback.

The native YouTube resolver handles watch/short/embed URLs whose public web or
Android player metadata exposes a direct MP4 or HLS URL. Signed JavaScript challenge and
sign-in-only streams return an error; the resolver does not bypass them.

The MA35H0 ADC touch panel reports ABS_X/ABS_Y/pressure, one contact, and no
multitouch slots. Genuine two-finger pinch is impossible on this panel. Drag
pans or scrolls, double tap toggles zoom, and the toolbar offers zoom, fit,
page navigation, full view/full screen and stop. Full screen retains an exit
button at the top right. The picture/PDF renderer keeps the original raster
for zoom rather than repeatedly enlarging a screen-sized thumbnail.

The `/tmp/muse-native-media.active` flag lets the legacy framebuffer writer
and touch helper yield while board-native media owns the display, including
the stopped/ended control UI. All backend subprocesses belong to the media
worker process group.

Install binaries and CA bundle/font in `/opt/muse-media`, assets beneath
`/opt/muse-media/assets`, and the unit at `/etc/systemd/system/muse-media.service`.
Use a compiler/sysroot matching the board's glibc 2.31 for networking backends;
newer static glibc builds can load incompatible on-board NSS libraries.
Third-party sources retain their licenses: nlohmann JSON (MIT), stb (MIT/public
domain), curl, OpenSSL, FFmpeg (configuration-dependent LGPL/GPL) and MuPDF
(AGPL/commercial). Native build artifacts are excluded from version control.

The renderer uses the white Muse layout, keeping windowed video in the left
chat pane alongside the thermostat and headphone controls. On this display,
1024x1200 virtual framebuffer storage provides two 1024x600 pages. Page
flipping is available only with MUSE_FB_PAGE_FLIP set; the default composes
a frame in RAM before copying it to the visible page.

VC8000 is a video decoder, while GC520 is the separate 2D graphics engine.
The H.264 V4L2 path is quarantined following a board hang during testing.
Requests with `hardware: true` return an error; loading a driver is not proof
of working acceleration. The current UI still
renders with the CPU. Neither accelerator module is enabled at boot by this
service. Software playback uses reduced FFmpeg thread queues and fixed ALSA
buffering, but its smoothness depends on the selected stream and available RAM.

Headphone PCM uses `hw:0,0`, stereo S16_LE at 48 kHz, with 1024-frame
periods and an 8192-frame buffer (8 whole periods). Do not replace these with
time-based buffer/period requests: the BSP's `ma35h0_prep_dma_cyclic` allocates
`buf_len / period_len` descriptors but loops while offset < buf_len. A partial
last period overruns that array and can hang the kernel, including Ethernet.
The included kernel patch rejects this geometry; it is prepared for BSP
integration and has not been installed on the board. Runtime avoids the case.

Buttons use the initial contact position despite resistive-panel jitter.
Drag handling is restricted to contacts that began in the media content area.
The systemd service caps memory at 38 MiB.

VC8000 playback is quarantined after a decoder-only test also made Ethernet
unresponsive. Hardware requests return an error; module presence does not mean
usable acceleration. The currently deployed fallback is CPU decoding. A frame
reader thread drains video independently of drawing, and audio supplies pacing
without an extra FFmpeg `-re` throttle. Frames may be dropped to preserve the
most recent picture. Video EOF retains the frame and continues servicing view
and control changes. Pause stops decoder/player processes while the UI remains
active. The full-screen exit has a large, jitter-tolerant target.

For the tested cached MP4, audio uses a separate FFmpeg process and direct
48 kHz stereo PCM playback. Video uses its own rate-limited decoder and a
separate frame reader. This keeps video drawing and decoding from starving
audio. The measured effective UI frame rate was about 15 fps at a requested
20 fps; this CPU fallback does not promise smooth full-resolution video.
The YouTube example was downloaded directly by the board into its local
asset directory, without a laptop downloader or decoder.

Audio DSP demo: the right panel has Room, 10 EQ, 5 HW EQ and DSP tabs. The
10-band equalizer runs stereo peaking filters on the CPU; the 5-band equalizer
runs inside the Nuvoton NAU8822 codec, using its ALSA `EQ Parameters` control.
Hardware EQ frequencies are 105/300/850/2400/6900 Hz, with integer gains from
-12 to +12 dB. Off sets those five hardware gains to unity and retains the
chosen gains in memory for the next On. It is not a hardware power-down.
Settings survive a new media item; in-memory bypassed gains do not survive a
service restart. The DSP tab controls CPU EQ, hardware EQ, DAC limiter, and
mic high-pass/automatic gain/noise gate. Mic switches affect recording only.
EQ Default resets both gain banks to flat, enables CPU EQ and bypasses the
hardware EQ; it leaves limiter and microphone controls unchanged.

The codec also supports 3D Stereo Enhancement (R41 depth 0..15), but this BSP
ALSA driver does not expose that control, so the demo does not enable it.
See the [Nuvoton NAU8822 datasheet](https://www.nuvoton.com/export/resource-files/NAU8822DataSheetRev3.3.pdf).
The spectrum is a 4096-point FFT of PCM after CPU EQ and before the hardware
codec: it cannot show the codec's subsequent EQ/limiter/3D processing.
Spectrum rendering can update without a new video frame. Playback end clears
bars and shows Replay; Stop retains the control UI and Replay starts the last
item. Standalone audio also uses the EQ and spectrum pipeline.

```json
{"action":"media.control","params":{"operation":"eq_view","value":2}}
{"action":"media.control","params":{"operation":"hardware_equalizer","band":0,"value":6}}
{"action":"media.control","params":{"operation":"dsp","feature":"hardware_eq","enabled":true}}
{"action":"media.control","params":{"operation":"dsp","feature":"software_eq","enabled":false}}
{"action":"media.control","params":{"operation":"dsp","feature":"limiter","enabled":true}}
{"action":"media.control","params":{"operation":"eq_default"}}
{"action":"audio.tune","params":{"preset":"bass_up","amount_db":3,"engine":"active"}}
```

`audio.tune` supports relative bass_up/bass_down/treble_up/treble_down, absolute
voice/music presets, and flat. The optional Muse adapter registers these
commands with phrase descriptions such as "I want more bass". They become
available to Muse only when that separate gateway runs and re-registers;
this native media service does not implement speech recognition or the Muse
chat transport. The gateway remains stopped for the ICS-only media demo.

Checks: `python -m pytest test_native_media.py test_audio_eq.py test_codec_dsp.py`.
