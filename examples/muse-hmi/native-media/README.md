# MA35 board-native media API

The API, HTTPS downloads, decoding, framebuffer rendering, touch gestures and
headphone playback run on the MA35 Linux board. Ethernet ICS supplies Internet
routing only. No laptop renderer, Python process or media proxy is required.
The native Muse chat gateway is deployed separately beside this renderer on
the board. BLE onboarding is not available there; it uses the existing pairing.

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

Downloads for documents/images are limited to 8 MiB. Finite video/audio may be
cached up to 64 MiB; continuous broadcasts are streamed.
Video output targets 480x270 at up to 20 fps with one decode thread. Input resolution,
codec and bitrate can still exceed the board's CPU/RAM budget: choose low
resolution streams. This is not a promise of smooth 1080p playback.

The native YouTube resolver handles watch/short/embed URLs whose public web or
Android player metadata exposes a direct MP4 or HLS URL. Signed JavaScript challenge and
sign-in-only streams return an error; the resolver does not bypass them.

The MA35H0 ADC touch panel reports ABS_X/ABS_Y/pressure, one contact, and no
multitouch slots. Genuine two-finger pinch is impossible on this panel. Drag
pans or scrolls, double tap zoom is disabled, and the toolbar offers zoom, fit,
page navigation, full view/full screen and stop. Full screen retains an exit
button at the top right. The picture/PDF renderer keeps the original raster
for zoom rather than repeatedly enlarging a screen-sized thumbnail.

The `/tmp/muse-native-media.active` flag lets the legacy framebuffer writer
and touch helper yield while board-native media is active. Stop/end removes
it. All backend subprocesses belong to the media worker process group.

For the five-tab native gateway, install binaries and CA bundle/font in
`/mnt/sd/muse/media`, assets beneath `/mnt/sd/muse/media/assets`, and the unit
at `/etc/systemd/system/muse-media.service`; see the parent README.
`/opt/muse-media` remains the standalone renderer default without environment overrides.
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
The H.264 V4L2 path is experimental and requires an explicit `hardware: true`
parameter. It is disabled by default following a board hang during testing;
loading a driver is not proof of working acceleration. The current UI still
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
chat transport. The earlier ICS-only demo kept the gateway stopped; the current deployment
runs its native gateway on the MA35 and retires the laptop gateway.

Checks: `python -m pytest test_native_media.py test_audio_eq.py test_codec_dsp.py`.

## SD runtime and hot tabs

The current deployment stores the native gateway, media runtime, downloads and
Piper/Kokoro assets under `/mnt/sd/muse`. `mnt-sd.mount`, `muse-swap.service`
and `muse-gateway.service` start them on the board. The test board used a
1 GiB swap file; this is much slower than physical RAM. The laptop gateway is
retired with `muse-bridge/state/board-native.enabled`. Ethernet/SSH forwarding
is still laptop passthrough. Existing pairing credentials remain private on SD.

Five tabs retain their last content. `media.switch` restores a tab's zoom, pan,
page and approximate playback position. Image/document cache is bounded to
8 MiB per item; finite audio/video cache is bounded to 64 MiB per item. Live
radio retains its selection and reconnects at the live edge. Double-tap zoom
is disabled; use the zoom buttons. Tab content survives a service restart;
view adjustments are saved when switching to another tab.

`media.ui` takes a `document` object with optional `title`, `subtitle`, `source`
and a `blocks` array. It draws structured generative UI in the Text tab:
metric cards, bar/line charts with multiple series, tables, progress and text.
Snapshots update an active text worker in place. Drag to scroll, use zoom or
full-screen controls, and tap chart points for values. Assistant text deltas
update at up to four snapshots per second. Completed `muse-ui` JSON code blocks
are accepted as an alternative to the registered display command. Ordinary
assistant commentary preserves an existing generated UI; explicit plain-text
`media.show` replaces it. The renderer does not execute HTML or scripts.

`sports.baseball` fetches MLB's official Stats API directly from the board and
builds a dashboard of actual team totals and home-run leaders. `team_id` defaults
to 133 (Athletics), `season` to 2026. Use 2024 for the last Oakland season.
Data is a snapshot fetched on request, not a scheduled background feed.

`audio.radio` selects Radio Paradise `paradise`, `mellow`, or `rock`.
`audio.stations` lists presets. Touching an empty Audio tab starts Main Mix.
Live broadcasts stream directly through FFmpeg, CPU EQ and the NAU8822; they
are never downloaded into an infinite cache file. All three streams returned
HTTP 200 and audio bytes on this board. SomaFM returned 403 on this connection
and was excluded from the presets.

The test board has Piper ARM64, Lessac low/medium voices and Kokoro v1.1 int8
with sherpa-onnx ARM64 installed on SD; they are not included in this source package. `speak` reads text from stdin and uses Piper low by default;
set `MUSE_TTS_ENGINE=kokoro` or `MUSE_PIPER_MODEL=en_US-lessac-medium` to select
another installed engine/model. Piper low produced and played a WAV on-board;
a 3.2-second test took roughly 24 seconds to load and 25 seconds to synthesize.
Kokoro's runtime starts and its model is installed, but its heavier synthesis
benchmark was stopped to free memory for interactive testing; audio synthesis
with Kokoro remains unverified. The SD swap improves capacity, not speed.

Build from `examples/muse-hmi` using the packaged Makefile. The renderer and
gateway must use the same AArch64 libc/toolchain as their backend/TLS libraries.
See the parent README for build and installation commands.
`deploy_board_native.py` replaces binaries atomically and restarts board services
without migrating or printing credentials.

## Muse GenUI v1 and native PDFs

The new `muse-genui` framework advertises `genui.present`, `genui.status` and `genui.capabilities` through the same command registration format as the Muse Gadgets Linux SDK. See [framework documentation](../genui/README.md). A presentation provides a sourced document, optional slideshow photo URLs and a YouTube/direct video URL. The gateway writes a native PDF from the document on the MA35 board and installs Text, Images, PDF and Video as one topic snapshot. Active Audio/radio and its cached station stay independent. Blank tabs are clickable and show their empty state.

`generated_report.hpp` writes PDF 1.4 directly in C++, including metric values, static charts, tables, pagination and source URLs. Reports live at `/mnt/sd/muse/media/assets/genui-report-*.pdf` and open with the existing on-board MuPDF backend. No laptop PDF process or cloud PDF service participates. The basic PDF font currently normalizes non-ASCII text. Image slides download lazily to distinct SD cache files; the board advances every one second by default and offers page controls. Cache keys include the process, timestamp and presentation revision to avoid reusing a previous topic after restart. Reports and context media remain on SD until explicitly cleaned up.

Text now has **Read to me** and **Stop voice** controls. Speech reads the visible
structured document (including metric, table and chart values), rather than the
assistant recap stored alongside it. Radio controls remain in Audio. The gateway
polls a board-local speech request every 200 ms; Stop terminates the synthesis and
playback process group. Piper streams raw PCM from full, file-backed text input
instead of replacing a WAV for each line. Text-button readback uses the installed eSpeak NG voice and streams audio as
soon as it becomes available. This voice is less natural than Piper, but avoids
Piper's measured 8–9x real-time synthesis cost on this board. Automatic Muse
speech retains Piper. Spoken values use spaces and continuous table rows, not
line breaks for each cell; Piper's extra sentence silence is disabled.

Decoded image and PDF pages are stored as validated RGB files on SD and mapped
on revisits, avoiding repeated downloads and PDF/PNG decoding. Brand assets are
resampled once at service startup. Tab hit targets tolerate finger movement and
near-edge touches. Image/PDF panning is constrained to the scaled content bounds,
so a fitted document cannot be dragged entirely out of view.


Presentation requests now show a dismissible readiness popup with Text, Images,
PDF and Video states. Source preparation runs in a low-priority child of the
board gateway, independently of tab touches and radio. Readiness verifies
every slideshow photo and the first PDF page and decodes one video frame. A failed source remains Error and cannot inflate the percentage
to 100. Tabs without supplied sources explicitly show No source. Generation IDs
reject late preparation updates from an older request. The popup closes three
seconds after all supplied tabs are ready. Natural requests such as “show me stuff
on the Yankees” trigger coordinated GenUI preparation from Muse user events.


The readiness popup shows elapsed time, the latest API activity and three recent
steps: request sent, HTTP acknowledgement, Muse research activity, connection
retry, GenUI arrival, PDF rasterizing, photo counts and YouTube frame checks.
Research activity does not invent a readiness percentage. Saved request state
survives a Muse reconnect. Commons photos use 960-pixel thumbnails; HTTP rate
limits get bounded retries in the native wrapper. The bundled curl retry option
can return success on a size error, so that option is deliberately avoided. Videos larger than the 64 MiB download cache
stream directly over HTTPS, with resolved YouTube URLs reused briefly on tab
revisits. Failed downloads remove their partial files.
