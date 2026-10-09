"""Muse GenUI v1: source-backed presentations rendered by a device backend."""
from __future__ import annotations
import json
import socket

__version__ = "0.1.0"
COMMAND_SPECS = {
 "genui.fetch":{"description":"Start direct board-side summary, photo and YouTube discovery in parallel for a topic, then generate the PDF and cache decoded media with a 60-second deadline. Use this FIRST for show me stuff requests, without doing cloud image searches or downloading images. Returns immediately; follow genui.status. The initial Wikipedia overview is source-labelled, not live news or live sports data; The initial board presentation is kept; later Muse updates for that request are ignored. A new user prompt starts a new presentation.","required":{"topic":{"type":"string","description":"The subject, e.g. Beijing, Seoul, New York Yankees or Meta Platforms."}},"optional":{},"timeout_ms":3000},
 "genui.present": {
  "description": "Present a fresh, sourced visual response for ANY topic: sports, news, companies, comparisons, plans or explanations. Look up current facts and relevant media, publish Text and PDF as soon as the sourced document is ready, with pending_media=true; do not wait for image/video searches. Send later presentations with the SAME document/title and additional media, setting pending_media=false when finished. Do not download photo or video bytes on Muse; send their real URLs and let the board fetch them. Text renders colorful charts/cards/tables; Images plays a slideshow of direct HTTPS photo URLs (use 10 relevant recent photos when requested); PDF is generated natively on the board from this document including sources; Video plays a relevant YouTube watch URL. All four visual tabs update together; Audio/radio selection stays independent. Missing image/video sources clear previous-topic content. Never invent factual values or media URLs. For latest news include dated headlines, summary and source links. Use genui.status readiness to verify actual rendering; saved tab parameters alone do not mean media is ready.",
  "required": {"document": {"type":"object", "description":"title, subtitle, source, sources:[{title,url}], theme?, team_id?, blocks:[...]. Types: metrics {items:[{title,value,unit?}]}; metric {title,value,unit?,detail?}; bar/line {title,labels:[strings],series:[{name,values:[numbers]}]}; table {title,columns:[strings],rows:[[strings/numbers]]}; progress {title,value,max?}; text {title,text}. Max 24 blocks, 48 chart points, 6 series, 100 table rows. Label dates and sample data. theme athletics or vivid; team_id=133 shows Athletics logo."}},
  "optional": {"pending_media":{"type":"boolean","description":"True for an early presentation while photos/video are still being gathered; missing media stay Waiting rather than being marked complete. Publish immediately, then update with real sources."},"version":{"type":"string","description":"Schema version, currently 1."},"slides":{"type":"array","description":"Up to 20 photos: [{url:direct HTTPS image URL,caption,source:article URL}]."},"interval_seconds":{"type":"integer","description":"Slideshow delay 1..60 seconds, default 1."},"video":{"type":"object","description":"{url:HTTPS YouTube watch URL or direct video URL, kind:youtube or video, caption}. Find a relevant actual video for this topic."}},
  "timeout_ms":15000,
 },
 "genui.status":{"description":"Read real board rendering state, errors and cached tabs.","required":{},"optional":{},"timeout_ms":10000},
 "genui.capabilities":{"description":"Read GenUI version, visual tabs, on-board PDF and slideshow limits.","required":{},"optional":{},"timeout_ms":10000},
}

class Client:
 """Uses the same run(command, params, timeout_ms) contract as Muse Executor."""
 def __init__(self, run):
  self.run = run
 def present(self, document, *, slides=None, video=None, interval_seconds=1, pending_media=False):
  params = {"version":"1", "document":document, "interval_seconds":interval_seconds, "pending_media":pending_media}
  if slides is not None: params["slides"] = slides
  if video is not None: params["video"] = video
  return self.run("genui.present", params, 15000)
 def fetch(self, topic):
  return self.run("genui.fetch", {"topic":topic}, 3000)
 def status(self):
  return self.run("genui.status", {}, 10000)

class Executor:
 """Composable Muse SDK executor; GenUI delegates to the board runtime."""
 def __init__(self, base, board_run):
  self.base, self.board_run = base, board_run
 def run(self, command, params, timeout_ms=None):
  if command in COMMAND_SPECS:
   return self.board_run(command, params, timeout_ms)
  return self.base.run(command, params, timeout_ms)

def device_description(*, node_id, display_name, commands=None):
 """Construct the SDK's actual DeviceDescription with GenUI commands added."""
 from musegadget.link_client import DeviceDescription
 return DeviceDescription(node_id, display_name, __version__, {**(commands or {}), **COMMAND_SPECS})

def unix_run(command, params, timeout_ms=None, *, path="/run/musegadget.sock"):
 """Local board transport. Never starts a gateway or handles pairing secrets."""
 with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
  sock.settimeout((timeout_ms or 15000)/1000)
  sock.connect(path)
  sock.sendall(json.dumps({"action":command,"params":params}).encode()+b"\n")
  data = bytearray()
  while b"\n" not in data:
   part = sock.recv(65536)
   if not part: break
   data.extend(part)
   if len(data)>1048576: raise ValueError("board response too large")
 return json.loads(data)
