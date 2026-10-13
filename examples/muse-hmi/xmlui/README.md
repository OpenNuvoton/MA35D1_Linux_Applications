# Muse smart-home XML demo: LVGL + Qt together

One component-style XML drives both toolkits in one process. LVGL renders the
base layer (navigation, controls and animated widgets); Qt renders transparent
cards, weather, news, charts and media over it. A single compositor owns the
1024×600 MA35H0 framebuffer. The XML format borrows LVGL's `component`, `api`,
`view`, reusable properties and widget attributes, and uses its actual runtime
XML parser for the LVGL layer. The Qt adapter interprets the same instantiated
widget tree. This is a documented shared subset, not a converter for every
possible LVGL XML feature.

The ten screens are **Home, Weather, News, Climate, Lighting, Energy, Security,
Air quality, Scenes, and Sound lab**. Each file in `xml/` contains both layers:
LVGL at `z=0`, transparent Qt at `z=1`. `components.xml` holds reusable `Action`
and `Metric` components. `screen.xsd` validates the files. XML changes take
effect when the service is restarted; AI content updates do not require a build.

Named properties use `$field` bindings. Muse selects a known template and supplies
its values through the device command API; it does not send executable code or
arbitrary XML. Initial scenarios are labelled SAMPLE. Partial AI updates replace
omitted factual metrics/headlines with “Not supplied” and require source labels
for sourced content. Home sensors and HVAC/light/security controls are examples;
no real home-device driver or sensor is implied. The NAU8822 audio controls are real.

## Muse commands and sample questions

- `xmlui.templates` returns template IDs, fields, labelled sample defaults, example questions and the configured home location.
- `xmlui.preferences` reads or saves a five-digit US ZIP code for relevant weather, local news and outdoor air-quality updates. The location persists in `preferences.json` on NAND; explicit location requests override it. Ask Muse buttons include that default in their prompts.
- `xmlui.present` binds a `template` and `data` object to both layers.
- `xmlui.status` reports the current template, bound data, frame count, measured frame rate and rendering times.
- `audio.autotune` guides user preference calibration; `audio.tone` generates a short listening tone on the board.

From the board:

```sh
/mnt/sd/muse/gateway/muse-gateway --local '{"action":"xmlui.templates"}'
/mnt/sd/muse/gateway/muse-gateway --local '{"action":"xmlui.present","params":{"template":"weather","data":{"hero":"An Austin evening","summary":"Example content only","metric1_label":"Temperature","metric1":"24 C","source":"SAMPLE / illustrative weather"}}}'
/mnt/sd/muse/gateway/muse-gateway --local '{"message":"Show current weather for my configured ZIP on my smart home display; choose an XML template and include sources.","xmlui":true}'
```

Other scenarios: “Find three current home-energy news stories,” “Suggest a warm
dinner lighting scene,” “Show illustrative home energy use,” “Build a good-night
routine,” and “Ask whether I want to calibrate my headphones.” Touch **Ask Muse
about this screen** to send the selected scenario to the existing native gateway.
No laptop chat/rendering gateway is started.

## Existing media remains available

The Photos, PDF, Video, Radio and Text buttons select the already cached native
media tabs. Slideshow timing, downloaded/raster caches, low-resolution video,
headphone playback, radio and PDF decoding continue in the existing media service.
When the XML compositor is active, that service publishes its pixel surface to
`/tmp/muse-xmlui-frame` with a sequence counter and yields framebuffer/touch
ownership. The Qt media widget composites the existing content area. Radio audio
continues independently of the visual template.

`xmlui.present` may include `presentation`, using the existing GenUI v1 document,
slides and YouTube/video schema. The app reuses the native PDF writer and asks
the native service to prepare the media. Documents and download caches remain
on SD; the demo binary, Qt libraries, XML and UI assets live on the newly mounted
NAND volume at `/mnt/nand/apps/smart-home-overlay`.

## Guided sound calibration

Muse first asks whether to calibrate. `audio.autotune` operation `start` records
the original hardware/software EQ and radio selection, but does not play sound.
Operations `a` and `b` replay the same generated 48 kHz stereo tone at the current
band frequency (105, 300, 850, 2400 or 6900 Hz). Tones are 1.5 seconds, -25 dBFS,
with 20 ms fades. Headphone volume is not changed. `choose` takes real listener
feedback `a`, `b` or `same`, with ten comparisons (3 dB coarse then 1 dB fine,
bounded to ±6 dB). Cancel restores the prior EQ/radio. Completion saves the
preferred profile and restores the prior radio selection.

This is a listening-preference procedure, not a microphone measurement of room
or headphone response. The user must actually listen and choose; Muse must not
invent responses or automatically run every comparison. The touch screen has
Start, Hear A, Hear B, Prefer A, Prefer B and Restore buttons.

`audio.tone` also supports individually requested 80–8000 Hz tones, 0.2–3 seconds,
and -36 to -18 dBFS. It uses the existing audio output and may replace an active
audio item; use the calibration flow when preserving radio is needed.

## Shared adaptive context

`adaptive.context`, `adaptive.configure`, `adaptive.update`, `adaptive.evaluate`,
`adaptive.publish`, `adaptive.refresh` and `adaptive.report` share one versioned
location/calendar/input record. A zip change invalidates old local facts and
requests a complete Muse refresh. Publications must match both zip and revision;
a late result for the old place cannot overwrite the new views. Resolve the new
IANA time zone with a sourced geocoder rather than retaining the previous zone.

Weather and air inputs expire after two hours; news after one day. Inputs retain
source, timestamp and measured/model/feed basis. Climate, lighting, energy,
purifier and security output is advice unless a real authorized adapter exists.
The demonstration has no thermostat, purifier, meter or security feeds attached.
The purifier proposes on above AQI 100, off below 50, and hold between them;
automation is disabled by default. These are demonstration policy values.

`adaptive.report` creates a five-page daily PDF from ten actual composed panels.
`adaptive.report.read` returns bounded chunks for an installed phone attachment
adapter. A local file path does not prove a report was delivered to a phone.
The optional timer is an explicit 08:00 Pacific schedule; adjust its OnCalendar
zone for the household. Panel dates and report metadata use the context zone.

## Build and verification

From `examples/muse-hmi/xmlui` (Qt 5 development tools, g++, Python and ffmpeg as needed):

```sh
python3 tools/fetch_lvgl.py
python3 tools/build_lvgl.py --arch host
mkdir -p build/qt-host
qmake smart_home.pro -o build/qt-host/Makefile LVGL_ARCH=host
make -C build/qt-host
./build/qt-host/smart-home-overlay --xml xml --folder . --self-test
python3 tests/check_api.py
```

LVGL 9.4 is pinned to upstream commit `5a59ad4cf5de8717e2c841e5c0e402803e42ab8b`;
`fetch_lvgl.py` installs it under the ignored `vendor/lvgl` directory. Existing
checkouts can be passed to `build_lvgl.py --lvgl-dir` and qmake `LVGL_SOURCE`. Qt 5 Widgets/XML/Network are used.
The host self-test renders ten actual composed PNGs in `build/previews`. The API
check exercises both layers, changing AI content, rejected bindings and animation
frame advancement. `tests/test_autotune.cpp` checks tone PCM, bounded comparisons,
fixed-volume commands and restoration with a fake media device.

For the board, build isolated LVGL objects with `--arch board`, run the BSP SDK
qmake with `LVGL_ARCH=board`, and run `tools/package_runtime.py`. It collects only
the required target Qt/standard C++ libraries and the offscreen plugin; existing
board glibc is used. `smart-home-overlay.service` supplies the library/plugin
paths, framebuffer access and NAND mount dependency. Source and dependencies
retain their respective Apache-2.0, LVGL MIT, and Qt/dependency licenses.

To return to the original five-tab screen, stop `smart-home-overlay.service`.
Its shutdown releases framebuffer ownership and requests a native redraw. Stop
an active calibration through Restore before intentionally leaving the screen.

## Rendering performance

LVGL redraws invalidated regions; Qt retains its layer and repaints animation,
media and pressed-control regions while preserving overlapping widgets. The
static glow is cached, composition blends only changed regions, and the 180-degree panel rotation runs in cached RAM
before one bulk framebuffer copy. Animations still run in both toolkits.
`xmlui.status` includes `fps` and per-frame `render_ms` values to distinguish
widget rendering from composition, rotation and framebuffer writes.

The self-test compares partial Qt updates to a full reference render on all ten
screens, including changing animation phases and pressing/releasing controls and changing every Home EQ/volume slider.
It allows at most 16 differing Qt edge pixels out of 614,400 for raster rounding;
partial composition must exactly match a complete blend. Animation targets 20 fps.
Internal maintenance checkpoints preserve calibration state without replaying
tones or submitting listener choices. VC8000 is a video/JPEG decoder; GC520L
is the separate 2D engine for blits, alpha blending and rotation. Neither is
required by this software compositor.


## MA35 board packaging and installation

Use the BSP SDK qmake and compiler matching the board libc. Native services must
also be rebuilt from the parent example with that toolchain. Qt libraries from
another sysroot are not interchangeable. From this directory:

```sh
python3 tools/build_lvgl.py --arch board --cc /path/to/bsp/bin/aarch64-linux-gcc
mkdir -p build/qt-board
/path/to/bsp/host/bin/qmake smart_home.pro -o build/qt-board/Makefile LVGL_ARCH=board
make -C build/qt-board
python3 tools/package_runtime.py --sysroot /path/to/bsp/target/sysroot
python3 tools/deploy_runtime.py --ssh-target root@BOARD --dry-run
python3 tools/deploy_runtime.py --ssh-target root@BOARD
```

First rebuild/install the parent native media/gateway with
`../native-media/deploy_board_native.py`. Its commands.json includes the new XML
and adaptive schemas. Pairing and existing media caches are retained. The XML
installer requires an already mounted `/mnt/nand`, uploads the runtime, brands,
font and IANA zoneinfo, then enables the compositor and timezone mount units.
Use `--daily-reports` only when the household wants the scheduled report refresh.
SSH port and existing relay HostKeyAlias are configurable in both installers.

`platform/nand` contains the exact board-specific non-erasing partition helper,
read-only inspection source and boot mount unit used for the 1 GiB test board.
It expects the existing `ubi1:appdata` volume in the 896 MiB tail and refuses a
mismatched partition geometry. Build against the matching vendor kernel. This
package does not run initial UBI formatting or include a precompiled module.

## Change the look through the shared XML

`themes/daybreak` is a bright, co-branded alternative: daylight background,
white metric cards, cobalt navigation and stronger teal/amber/purple values.
Both toolkits consume these same templates. Fields, actions and coordinates
are unchanged, so existing Muse context and media controls still bind normally.

```sh
python3 tools/apply_theme.py
./build/qt-host/smart-home-overlay --xml themes/daybreak --folder . --self-test
python3 tools/package_runtime.py --sysroot /path/to/bsp/target/sysroot --xml-dir themes/daybreak
```

Reload the overlay after changing XML. Ask Muse to refresh the current context
through `adaptive.refresh`, or bind current sourced content through
`xmlui.present`; media remains managed by the native service. Muse can choose
known templates and bounded content, while the XML owns the visual composition.

## Demo collateral

The [architecture prospectus and 30-second reel](../docs/efficiency-adaptive-home/README.md)
include ten real board captures, cropped feature details and Winbond/Nuvoton
branding. The reel uses an existing Kokoro voice, newly synthesized music,
matching SRT captions and a seeded camera/music generator. See
[LICENSES.md](LICENSES.md) for dependency and asset provenance.
