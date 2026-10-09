# Muse GenUI

A new API framework layered on the Meta Muse Gadgets Linux SDK. Muse retrieves facts and media for a request and submits a version 1 presentation. The MA35 backend downloads and renders content, caches tab state on SD, and generates a PDF in native C++. The laptop provides network passthrough; the paired gateway runs on the board.

```python
from muse_genui import Client, unix_run

ui = Client(unix_run)  # on the board; SDK clients can pass their Executor.run
ui.fetch("Beijing")  # returns immediately; check ui.status() for readiness
ui.present(
    {"title": "My sourced response", "subtitle": "Date and context",
     "sources": [{"title": "Original source", "url": "https://example.org/report"}],
     "blocks": [{"type": "text", "title": "Summary", "text": "Source-backed facts"}]},
    slides=[{"url": "https://example.org/photo.jpg", "caption": "Photo", "source": "https://example.org/report"}],
    video={"url": "https://www.youtube.com/watch?v=ACTUAL_VIDEO_ID", "kind": "youtube"},
)
```

The example URLs are placeholders; producers must supply actual retrieved URLs. `genui.present` replaces Text, Images, PDF and Video together. Radio selection and active audio playback remain independent. Omitted media in a same-topic refinement keep the prepared images/video and active view, even when the display title changes. Wikipedia source URLs identify those refinements. Explicit empty slides clear the slideshow; genuinely new topics clear old media. Set pending_media=True to publish Text/PDF before sources arrive. Every accepted presentation gets its own generated PDF under `/mnt/sd/muse/media/assets/`. `genui.status` reports actual rendering, including errors, rather than treating an accepted command as successful playback.

For SDK integration, add `COMMAND_SPECS` to `DeviceDescription.commands` (or use `device_description`) and pass `Executor(base_executor, board_run).run` as `LinkSession.run_command`. Existing SDK pairing, Noise, registration and invocation are reused. The current board uses the native equivalent of that SDK transport; Python is optional for producer tools and is not required on the board. No SDK credential format changes are needed.

The board validates bounded blocks, URLs and numeric values before updating a presentation. The native renderer supports metric grids, individual metrics, bar/line charts, tables, progress and text. Slides are direct HTTPS photographs, up to 20, with 1–60 second delay (one second by default); YouTube uses the installed resolver and public downloadable playback where available. Media retrieval remains asynchronous. The native gateway starts direct board discovery for visual requests such as "show me stuff on Beijing"; genui.fetch also exposes it through the SDK. Summary, Commons photo URLs, and public YouTube results are fetched in parallel, without waiting for cloud image searches. The initial overview labels Wikipedia as its source and distinguishes article photos from recent news. For board-managed visual prompts, the first presentation is retained; late Muse presentations and assistant text updates are ignored. Only a new user prompt starts another topic. This preference survives gateway restarts. Standalone SDK presentations remain available through the local board API. Quick discovery uses a 60-second preparation deadline (30 seconds remains the performance target) and streams video rather than downloading an entire movie. Errors remain visible rather than being reported as successful readiness.

The native PDF includes metrics, tables, chart values and source URLs. It uses built-in PDF fonts and currently normalizes non-ASCII text; full Unicode typography is a future extension. Documents and media cache stay on SD; active decoding is bounded for the board's small RAM.

The native backend also exposes `readiness` in `genui.status`: a real percentage,
a request generation and separate Text/Images/PDF/Video states. `cached: true`
means a tab has parameters saved; it does not establish successful rendering.
Background preparation validates every slideshow image and the first PDF page and decodes the
first video frame. Only successful preparation marks a source Ready. Failed
sources remain Error; absent sources say Unavailable. Prepared slideshow images are cached on SD. The board shows the same progress in a dismissible popup.

Measured on the MA35H0 for a fresh Beijing request on 2026-10-09: Text 1.04 s, generated PDF 2.58 s, all ten photos decoded 7.15 s, first YouTube frame verified 10.93 s. These are observed timings, not a guarantee for every network/source.

HTTP 429 and transient server errors retry only the failed media download, with bounded backoff inside the preparation deadline. The gallery and document are not reset for those retries.
