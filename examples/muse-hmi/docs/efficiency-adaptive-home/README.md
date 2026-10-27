# A New Dawn for Smart Home AI

The [branded PDF](A_New_Dawn_for_Smart_Home_AI.pdf) is an eleven-page landscape
architecture prospectus with Winbond/Nuvoton branding and crops of ten actual
MA35 display captures. The [full paper](A_New_Dawn_for_Smart_Home_AI.md) retains
the context model, API contracts, evidence boundaries and evaluation plan.

The [30-second HD reel](Winuvo_30s_Remix.mp4) uses Kokoro narration, an original
cinematic/electronic score and feature-matched captions. Its
[SRT captions](Winuvo_30s_Remix.srt) and [timed storyboard](reel-storyboard.json)
record exactly which captured screen accompanies each spoken line. Security
is presented as ready for feeds; the capture correctly shows them disconnected.
Air values remain labelled estimates and energy savings are not claimed.

Regenerate the PDF with Python, Pillow and PyMuPDF:

```sh
python3 build_ad_brochure.py
```

Regenerate the reel from `examples/muse-hmi/xmlui` with ffmpeg and the packages
in `demo-video/requirements-demo.txt`. Supply your existing Kokoro v1.0 model
and voices, or fetch the model assets from the
[kokoro-onnx model-files-v1.0 release](https://github.com/thewh1teagle/kokoro-onnx/releases/tag/model-files-v1.0).
Weights are deliberately separate from the source package.

```sh
python3 demo-video/build_adaptive_home_remix.py --seed 847410703 \
  --kokoro-model /path/to/kokoro-v1.0.int8.onnx \
  --kokoro-voices /path/to/voices-v1.0.bin
```

The seed recreates camera choices and the new music. Each caption is the actual
spoken text and stays on its corresponding screen; multi-screen remixing occurs
only during the closing brand line. The output is H.264/AAC, 1920×1080, 30 fps,
with both video and audio spanning exactly 30 seconds.
