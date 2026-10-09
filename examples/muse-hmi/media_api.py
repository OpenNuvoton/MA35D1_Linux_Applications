# SPDX-License-Identifier: Apache-2.0
"""Optional Muse gateway adapter to the independent on-board media API."""
import json
import shlex
import subprocess
from musegadget.executor import error

MEDIA_SPECS = {
 'media.show': {'description':'Single-tab media command. Prefer genui.present for any topic or multi-media response to update all visual tabs together. Present media using the MA35 board-native renderer. All downloads and decoding run on the board, not this gateway. For public photos use kind=image with the original directly downloadable HTTPS image URL; the board fetches it itself, so uploading a copy to Muse storage is unnecessary. Query media.capabilities first. kind: auto, text, image, animation, pdf, svg, audio, video, youtube. YouTube direct-stream availability depends on the public player. Acceptance is asynchronous; poll media.status to verify playback.', 'required':{}, 'optional':{'kind':{'type':'string','description':'Media kind; auto detects known URL extensions.'},'url':{'type':'string','description':'HTTPS URL or /opt/muse-media/assets/ file.'},'text':{'type':'string','description':'Text to show.'},'caption':{'type':'string','description':'Display title.'},'page':{'type':'integer','description':'Document page, starting at 1.'},'fullscreen':{'type':'boolean','description':'Start in full screen.'},'audio':{'type':'boolean','description':'Play video audio, default true.'}},'timeout_ms':10000},
 'media.control': {'description':'Control current board-native media. operation: stop, replay, pause, resume, fullscreen, zoom, pan, page, fit, slideshow_interval, equalizer, hardware_equalizer, eq_flat, eq_default, eq_view, dsp. Ten-band EQ is software; five-band EQ is NAU8822 codec hardware. EQ default resets both banks flat. For DSP specify feature software_eq, hardware_eq, limiter, mic_hpf, mic_alc or mic_gate and enabled. Mic switches affect recording only. Slideshow interval uses value 1..60 seconds. Zoom 1..4; pan uses screen pixels; full screen has a touch exit button.', 'required':{'operation':{'type':'string','description':'Playback or view operation.'}}, 'optional':{'enabled':{'type':'boolean','description':'Full screen or DSP switch state.'},'value':{'type':'number','description':'Slideshow delay in seconds (1..60), zoom, document page, EQ gain in dB (-12..12), or view 0 Room / 1 CPU EQ / 2 codec EQ / 3 DSP.'},'band':{'type':'integer','description':'EQ band index 0..9 software or 0..4 hardware.'},'feature':{'type':'string','description':'DSP switch: software_eq, hardware_eq, limiter, mic_hpf, mic_alc, mic_gate.'},'x':{'type':'number','description':'Horizontal pan.'},'y':{'type':'number','description':'Vertical pan.'}},'timeout_ms':10000},
 'media.status': {'description':'Read actual on-board media state, rendering count and errors. An accepted media.show is not proof of playback.', 'required':{},'optional':{},'timeout_ms':10000},
 'media.capabilities': {'description':'Read the board media backends, formats, touch support and actual playback limits. Does not claim universal format support.', 'required':{},'optional':{},'timeout_ms':10000},
}

MEDIA_SPECS['display.picture'] = {
 'description': 'Single-photo-only command. For requests to see stuff about ANY topic, use genui.present instead so Text, Images, PDF and Video update together; do not send photos one at a time. Show a public photograph or an already hosted generated image on the MA35 board. For requests such as "president pic" or "show me a picture", find a directly downloadable public HTTPS image URL and call this command. The board downloads and decodes the original URL. No Muse storage upload or creation of a new public file link is needed. This command does not grant or bypass Muse permissions. Poll media.status to confirm rendering.',
 'required': {'url': {'type':'string','description':'Direct HTTPS image URL, not a webpage or local VM file.'}},
 'optional': {'caption':{'type':'string','description':'Short title or source attribution.'},'fullscreen':{'type':'boolean','description':'Start full screen, default false.'}},
 'timeout_ms':10000,
}

MEDIA_SPECS["media.enqueue"] = dict(MEDIA_SPECS["media.show"], description="Queue media on the board after current playback ends. Queue runs locally with no gateway required.")

MEDIA_SPECS['audio.tune'] = {
 'description': 'Adjust the MA35 board sound in response to spoken or typed requests. Map "I want more bass", "more low end", "make it bassier" to bass_up; "less bass" or "too boomy" to bass_down; "brighter", "more treble" to treble_up; "less treble", "too harsh" to treble_down; "clearer speech" to voice; "music preset" to music; "reset EQ", "default sound", "flat EQ" to flat. Bass/treble changes are relative and clamped to +/-12 dB. Call media.status to verify. Use media.control operation dsp for phrases such as "turn on the codec limiter" or "disable hardware EQ". The spectrum measures PCM before codec hardware effects.',
 'required': {'preset': {'type':'string','description':'bass_up, bass_down, treble_up, treble_down, voice, music or flat.'}},
 'optional': {'amount_db':{'type':'number','description':'Relative tone change, 0..12 dB, default 3.'},'engine':{'type':'string','description':'active (default), software (10 bands), or hardware (5 NAU8822 bands). Active selects hardware only when hardware EQ is on and software EQ is off.'}},
 'timeout_ms':10000,
}

def call_media(ssh,action,params):
 if action=='display.picture':
  from urllib.parse import urlsplit
  if not isinstance(params,dict) or set(params)-{'url','caption','fullscreen'}:return error('invalid picture parameters')
  url=params.get('url')
  if not isinstance(url,str) or urlsplit(url).scheme!='https' or not urlsplit(url).hostname:return error('a direct HTTPS image URL is required')
  caption=params.get('caption','Muse picture')
  if not isinstance(caption,str) or len(caption)>180:return error('caption must be text of at most 180 characters')
  if 'fullscreen' in params and type(params['fullscreen']) is not bool:return error('fullscreen must be boolean')
  action='media.show'
  params={'kind':'image','url':url,'caption':caption,'fullscreen':params.get('fullscreen',False)}
 try:
  request=json.dumps({'action':action,'params':params})
  if action.startswith('genui.') or action in ('sports.baseball','speech.read_text','speech.stop'):
   # These commands need gateway orchestration (PDF generation, discovery or TTS).
   command='/mnt/sd/muse/gateway/muse-gateway --local '+shlex.quote(request)
   proc=subprocess.run(ssh+[command],capture_output=True,timeout=65 if action=='sports.baseball' else 20)
  else:
   command='/opt/muse-media/curl -sS --max-time 8 -H "Content-Type: application/json" --data-binary @- http://127.0.0.1:8765/media'
   proc=subprocess.run(ssh+[command],input=request.encode(),capture_output=True,timeout=12)
  if proc.returncode:return error('on-board media API unavailable')
  return json.loads(proc.stdout)
 except (OSError,ValueError,subprocess.TimeoutExpired):return error('on-board media API unavailable or invalid response')

MEDIA_SPECS['media.ui'] = {
 'description': 'Show dynamic generated UI on the Text tab. When asked for stats, comparisons, trends or progress, fetch accurate data then call this command to present graphics. Replace the document to update it live. Do not invent real stats. Include source and date in subtitle/source; label sample data explicitly. Charts support bar/line, multiple series, negative values, scrolling, zoom and tap values. UI stays cached across tab switches.',
 'required': {'document': {'type':'object','description':'JSON object: title?, subtitle?, source?, blocks:[...]. Block types: metric {title,value,unit?,detail?}; bar or line {title,labels:[strings],series:[{name,values:[numbers]}],unit?}; table {title,columns:[strings],rows:[[strings/numbers]]}; progress {title,value,max?,unit?}; text {title,text}. Max 24 blocks, 48 points/chart, 6 series, 8 table columns and 100 rows. Values and labels must have matching lengths. Example: {"title":"Baseball comparison","subtitle":"Example data only","blocks":[{"type":"bar","title":"Home runs","labels":["Player A","Player B"],"series":[{"name":"HR","values":[20,32]}]}]}'}},
 'optional': {'background': {'type':'boolean','description':'Cache an update without switching away from another active tab.'}},
 'timeout_ms':10000
}
MEDIA_SPECS['media.show']['optional']['live']={'type':'boolean','description':'True for continuous live audio/radio; stream directly, never download the entire broadcast.'}

MEDIA_SPECS['audio.radio']={'description':'Play free Internet radio directly on the board Audio tab, with headphone EQ and live spectrum. Station choices: paradise (eclectic main), mellow or rock. Broadcast is streamed, not cached as an infinite file; returning to Audio reconnects the selected station.', 'required':{},'optional':{'station':{'type':'string','description':'paradise, mellow or rock'}},'timeout_ms':10000}
MEDIA_SPECS['audio.stations']={'description':'List built-in free radio station presets.','required':{},'optional':{},'timeout_ms':10000}

MEDIA_SPECS['sports.baseball']={'description':'Fetch actual MLB baseball stats directly on the board and show charts, stat cards and player comparison tables in Text. For Oakland A/Athletics requests use team_id=133. Fetches fresh official MLB data, not sample values. Default latest season 2026; Oakland final season is 2024. Ask for season when needed.','required':{},'optional':{'team_id':{'type':'integer','description':'MLB team ID, default 133 Athletics.'},'season':{'type':'integer','description':'MLB season year, default 2026.'}},'timeout_ms':60000}

# Advertise the same API as producer tools built on the Muse Linux SDK.
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent / 'genui' / 'src'))
from muse_genui import COMMAND_SPECS as GENUI_SPECS
MEDIA_SPECS.update(GENUI_SPECS)

MEDIA_SPECS["speech.read_text"]={"description":"Read the current Text panel aloud on board headphones, including generated cards, tables and charts. Does not replace the Audio tab or radio station.","required":{},"optional":{},"timeout_ms":10000}
MEDIA_SPECS["speech.stop"]={"description":"Stop Text panel speech immediately, including voice synthesis in progress. Keeps the radio selection.","required":{},"optional":{},"timeout_ms":10000}
