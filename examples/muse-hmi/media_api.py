"""Optional Muse gateway adapter to the independent on-board media API."""
import json
import subprocess
from musegadget.executor import error

MEDIA_SPECS = {
 'media.show': {'description':'Present media using the MA35 board-native renderer. All downloads and decoding run on the board, not this gateway. Query media.capabilities first. kind: auto, text, image, animation, pdf, svg, audio, video, youtube. YouTube direct-stream availability depends on the public player. Acceptance is asynchronous; poll media.status to verify playback.', 'required':{}, 'optional':{'kind':{'type':'string','description':'Media kind; auto detects known URL extensions.'},'url':{'type':'string','description':'HTTPS URL or /opt/muse-media/assets/ file.'},'text':{'type':'string','description':'Text to show.'},'caption':{'type':'string','description':'Display title.'},'page':{'type':'integer','description':'Document page, starting at 1.'},'fullscreen':{'type':'boolean','description':'Start in full screen.'},'audio':{'type':'boolean','description':'Play video audio, default true.'}},'timeout_ms':10000},
 'media.control': {'description':'Control current board-native media. operation: stop, replay, pause, resume, fullscreen, zoom, pan, page, fit, equalizer, hardware_equalizer, eq_flat, eq_default, eq_view, dsp. Ten-band EQ is software; five-band EQ is NAU8822 codec hardware. EQ default resets both banks flat. For DSP specify feature software_eq, hardware_eq, limiter, mic_hpf, mic_alc or mic_gate and enabled. Mic switches affect recording only. Zoom 1..4; pan uses screen pixels; full screen has a touch exit button.', 'required':{'operation':{'type':'string','description':'Playback or view operation.'}}, 'optional':{'enabled':{'type':'boolean','description':'Full screen or DSP switch state.'},'value':{'type':'number','description':'Zoom, document page, EQ gain in dB (-12..12), or view 0 Room / 1 CPU EQ / 2 codec EQ / 3 DSP.'},'band':{'type':'integer','description':'EQ band index 0..9 software or 0..4 hardware.'},'feature':{'type':'string','description':'DSP switch: software_eq, hardware_eq, limiter, mic_hpf, mic_alc, mic_gate.'},'x':{'type':'number','description':'Horizontal pan.'},'y':{'type':'number','description':'Vertical pan.'}},'timeout_ms':10000},
 'media.status': {'description':'Read actual on-board media state, rendering count and errors. An accepted media.show is not proof of playback.', 'required':{},'optional':{},'timeout_ms':10000},
 'media.capabilities': {'description':'Read the board media backends, formats, touch support and actual playback limits. Does not claim universal format support.', 'required':{},'optional':{},'timeout_ms':10000},
}

MEDIA_SPECS["media.enqueue"] = dict(MEDIA_SPECS["media.show"], description="Queue media on the board after current playback ends. Queue runs locally with no gateway required.")

MEDIA_SPECS['audio.tune'] = {
 'description': 'Adjust the MA35 board sound in response to spoken or typed requests. Map "I want more bass", "more low end", "make it bassier" to bass_up; "less bass" or "too boomy" to bass_down; "brighter", "more treble" to treble_up; "less treble", "too harsh" to treble_down; "clearer speech" to voice; "music preset" to music; "reset EQ", "default sound", "flat EQ" to flat. Bass/treble changes are relative and clamped to +/-12 dB. Call media.status to verify. Use media.control operation dsp for phrases such as "turn on the codec limiter" or "disable hardware EQ". The spectrum measures PCM before codec hardware effects.',
 'required': {'preset': {'type':'string','description':'bass_up, bass_down, treble_up, treble_down, voice, music or flat.'}},
 'optional': {'amount_db':{'type':'number','description':'Relative tone change, 0..12 dB, default 3.'},'engine':{'type':'string','description':'active (default), software (10 bands), or hardware (5 NAU8822 bands). Active selects hardware only when hardware EQ is on and software EQ is off.'}},
 'timeout_ms':10000,
}

def call_media(ssh,action,params):
 try:
  proc=subprocess.run(ssh+['/opt/muse-media/curl -sS --max-time 8 -H "Content-Type: application/json" --data-binary @- http://127.0.0.1:8765/media'],input=json.dumps({'action':action,'params':params}).encode(),capture_output=True,timeout=12)
  if proc.returncode:return error('on-board media API unavailable')
  return json.loads(proc.stdout)
 except (OSError,ValueError,subprocess.TimeoutExpired):return error('on-board media API unavailable or invalid response')
