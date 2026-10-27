# SPDX-License-Identifier: Apache-2.0
"""Render streamed Muse chat to the MA35H0 framebuffer over its Ethernet relay."""
import functools
import collections
import io
import logging
from pathlib import Path
import subprocess
import threading
import time
from PIL import Image, ImageDraw, ImageFont, ImageSequence
from speech_output import SpeechOutput
from thermostat_demo import ThermostatDemo
from framebuffer_writer import FramebufferWriter
from image_display import image_url_from_text

ROOT = Path(__file__).resolve().parent
log = logging.getLogger(__name__)
W, H = 1024, 600
FONT_PATH = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'

class ChatDisplay:
    def __init__(self, ssh):
        self.ssh = ssh
        self.writer = FramebufferWriter(ssh)
        self.preview_saved = 0
        self.written_scroll = 0
        self.speaker = SpeechOutput(ssh)
        self.volume = self.speaker.volume
        self.muted = self.speaker.muted
        self.chat_image = None
        self.image_caption = ""
        self.messages = collections.OrderedDict()
        self.seen = collections.deque(maxlen=2048)
        self.status = 'Connecting to Muse'
        self.voice_state = 'idle'
        self.mic_level = 0.0
        self.waiting_confirmation = False
        self.setpoint = 72
        self.mode = 'Auto'
        self.thermostat = ThermostatDemo()
        self.lock = threading.Lock()
        self.dirty = threading.Event()
        self.closed = threading.Event()
        self.previous = None
        self.scroll_key = None
        self.scroll_offset = 0.0
        self.scroll_updated = time.monotonic()
        self.scroll_resume = self.scroll_updated + 3
        self.frames = 0
        self.events = 0
        self.font = ImageFont.truetype(FONT_PATH, 18)
        self.small = ImageFont.truetype(FONT_PATH, 17)
        self.title = ImageFont.truetype(FONT_PATH, 35)
        # Optional user-supplied artwork; no third-party artwork is bundled.
        self.logo = Image.new('RGBA', (70, 70), '#18233f')
        ImageDraw.Draw(self.logo).text((17, 12), 'M', font=self.title, fill='white')
        if (ROOT / 'muse-logo.svg').exists():
            import cairosvg
            png = cairosvg.svg2png(url=str(ROOT / 'muse-logo.svg'), output_width=70, output_height=70)
            self.logo = Image.open(io.BytesIO(png)).convert('RGBA')
        bot = Image.new('RGBA', (78, 78))
        draw = ImageDraw.Draw(bot)
        draw.rounded_rectangle((9, 13, 69, 65), radius=12, fill='#635bff')
        draw.ellipse((22, 27, 30, 35), fill='white')
        draw.ellipse((48, 27, 56, 35), fill='white')
        draw.arc((23, 33, 55, 55), 0, 180, fill='white', width=3)
        self.avatar_frames = [bot]
        if (ROOT / 'jollybot.gif').exists():
            avatar = Image.open(ROOT / 'jollybot.gif')
            self.avatar_frames = [frame.convert('RGBA').resize((78,78), Image.Resampling.LANCZOS) for frame in ImageSequence.Iterator(avatar)]
        self.thread = threading.Thread(target=self._render_loop, daemon=True, name='board-display')
        self.thread.start()
        self.dirty.set()

    def set_status(self, status):
        with self.lock:
            self.status = status
        self.dirty.set()

    def event(self, event):
        name, mid, text = event['event'], event['message_id'], event['text']
        if name=='task.status':
            self.waiting_confirmation=event.get('status')=='pending_user_confirmation'
            self.set_status('Approval needed in Muse app' if self.waiting_confirmation else 'Muse is working' if event.get('status') in ('running','in_progress') else 'Live chat')
            return
        if name=='approvals.snapshot':
            self.waiting_confirmation=bool(event.get('pending_approvals'))
            self.set_status('Approval needed in Muse app' if self.waiting_confirmation else 'Live chat')
            return
        if name not in ('message.user', 'message.assistant', 'delta.message_start', 'delta.text_append', 'delta.message_done', 'delta.presentation'):
            return
        if not mid:
            return
        key = (name, mid, event.get('seq'))
        with self.lock:
            if event.get('seq') is not None and key in self.seen:
                return
            self.seen.append(key)
            role = 'You' if name == 'message.user' else 'Muse'
            item = self.messages.setdefault(mid, {'role': role, 'text': '', 'streaming': False})
            if name == 'delta.text_append':
                item['text'] += text
                item['streaming'] = True
            elif name == 'delta.message_start':
                item['streaming'] = True
            else:
                if text:
                    if name=='delta.presentation' and item['text'] and not image_url_from_text(text):
                        pass  # Caption must not erase the streamed image link.
                    else:item['text'] = text
                item['streaming'] = False
            while len(self.messages) > 60:
                self.messages.popitem(last=False)
            self.status = 'Approval needed in Muse app' if getattr(self,'waiting_confirmation',False) else 'Muse is replying' if item['streaming'] else 'Waiting for Muse' if role=='You' else 'Live chat'
            self.events += 1
        if self.events <= 5 or name in ('message.user', 'delta.message_done', 'message.assistant'):
            log.info('chat display event %s: %d text characters', name, len(text))
        speaker=getattr(self,'speaker',None)
        if speaker:
            if name=='message.user': speaker.cancel()
            elif not image_url_from_text(item['text']): speaker.update(mid,item['text'],not item['streaming'])
        self.dirty.set()

    @functools.lru_cache(maxsize=128)
    def _wrap(self, text, width):
        lines = []
        for paragraph in text.replace('\r', '').split('\n'):
            line = ''
            for word in paragraph.split(' '):
                trial = (line + ' ' + word).strip()
                if self.font.getlength(trial) <= width:
                    line = trial
                    continue
                if line:
                    lines.append(line)
                line = word
                while self.font.getlength(line) > width:
                    n = max(1, int(len(line)*width/self.font.getlength(line)))
                    lines.append(line[:n])
                    line = line[n:]
            lines.append(line)
        return lines or ['']

    def tap(self, x, y):
        # Coordinates refer to the upright layout; touch input compensates for rotation.
        if 24<=x<=674 and 112<=y<=504 and getattr(self,'chat_image',None) is not None:
            with self.lock:self.chat_image=None
            self.dirty.set();return 'image.clear'
        if 474 <= x <= 658 and 524 <= y <= 583:
            return 'speech.stop'
        if 20 <= x <= 452 and 520 <= y <= 589:
            return 'voice'
        if 688 <= x <= 1023 and 505 <= y <= 599:
            with self.lock:
                if x<=782:self.volume=max(0,self.volume-5);self.muted=False
                elif x>=912:self.volume=min(100,self.volume+5);self.muted=False
                else:self.muted=not self.muted
            self.dirty.set()
            return 'volume'
        with self.lock:
            if 724 <= x <= 816 and 374 <= y <= 438:
                self.setpoint = max(50, self.setpoint-1)
            elif 876 <= x <= 976 and 374 <= y <= 438:
                self.setpoint = min(90, self.setpoint+1)
            elif 711 <= x <= 985 and 450 <= y <= 495:
                modes = ['Off','Heat','Cool','Auto']
                self.mode = modes[(modes.index(self.mode)+1)%4]
            else:
                return None
        self.dirty.set()
        return 'temperature'

    def set_mic_level(self, level):
        with self.lock:self.mic_level = level
        self.dirty.set()

    def set_voice(self, state):
        with self.lock:
            self.voice_state = state
        self.dirty.set()

    def render(self):
        with self.lock:
            messages = [dict(m) for m in self.messages.values()]
            message_key = next(reversed(self.messages),None)
            chat_image = self.chat_image
            image_caption = self.image_caption
            waiting = self.waiting_confirmation
            volume,muted = self.volume,self.muted
            mic_level = self.mic_level
            thermostat = self.thermostat.snapshot(self.setpoint,self.mode)
            status, voice, target, mode = self.status, self.voice_state, self.setpoint, self.mode
        img = Image.new('RGB', (W, H), '#f4f6fc')
        d = ImageDraw.Draw(img)
        d.rectangle((0, 0, W, 96), fill='white')
        img.paste(self.logo, (24,12), self.logo)
        d.text((109,17),'Muse',font=self.title,fill='#18233f')
        d.text((110,61),'Talk, chat, and make yourself at home',font=self.small,fill='#6a7590')
        avatar=self.avatar_frames[int(time.monotonic()*5)%len(self.avatar_frames)]
        img.paste(avatar,(656,9),avatar)
        d.ellipse((765,32,777,44),fill='#34b88c' if status != 'Connecting to Muse' else '#d9a443')
        d.text((787,25),'Approval needed' if waiting else status,font=self.small,fill='#465878')
        entries=[]
        # Show the newest turn in full, preserving every wrapped line.
        selected=messages[-2:] if messages and messages[-1]['role']=='Muse' else messages[-1:]
        for m in selected:
            text=m['text'] or ('Thinking…' if m['streaming'] else '')
            if not text: continue
            lines=self._wrap(text,570)
            entries.append((m,lines,36+24*len(lines)))
        if not entries:
            d.text((32,165),'Hello. Let’s talk.',font=self.title,fill='#24324f')
            d.text((34,226),'Tap the microphone to talk to Muse.',font=self.font,fill='#61708b')
        else:
            viewport_height=380
            total=sum(e[2]+12 for e in entries)
            maximum=max(0,total-viewport_height)
            key=message_key
            now=time.monotonic()
            if key!=self.scroll_key:
                self.scroll_key=key;self.scroll_offset=0.0
                self.scroll_resume=now+3
            elapsed=min(0.5,max(0,now-self.scroll_updated))
            self.scroll_updated=now
            if now>=self.scroll_resume:
                self.scroll_offset=min(maximum,self.scroll_offset+elapsed*22)
            self.scroll_offset=min(maximum,self.scroll_offset)
            pane=Image.new('RGB',(650,viewport_height),'#f4f6fc')
            pd=ImageDraw.Draw(pane)
            y=-int(self.scroll_offset)
            for m,lines,height in entries:
                user=m['role']=='You';x=10 if user else 0
                if y+height>=0 and y<viewport_height:
                    pd.rounded_rectangle((x,y,634,y+height),radius=15,fill='#dee7ff' if user else 'white')
                    pd.text((x+14,y+6),m['role']+(' · replying' if m['streaming'] else ''),font=self.small,fill='#3761c4' if user else '#547096')
                    for n,line in enumerate(lines):
                        row=y+30+n*24
                        if -24<row<viewport_height:
                            pd.text((x+14,row),line,font=self.font,fill='#172747')
                y+=height+12
            img.paste(pane,(24,124))
            if maximum:
                pd_label=f"Auto scrolling · {int(self.scroll_offset/maximum*100)}%"
                d.text((28,101),pd_label,font=self.small,fill='#72819a')
        if chat_image is not None:
            d.rectangle((24,100,674,504),fill='#f4f6fc')
            d.rounded_rectangle((24,112,674,504),radius=16,fill='white')
            img.paste(chat_image,(24+(650-chat_image.width)//2,124+(348-chat_image.height)//2))
            d.text((38,478),image_caption[:75] or 'Image · tap to return to chat',font=self.small,fill='#547096')
        # Room temperature and HVAC state are explicitly simulated.
        d.rounded_rectangle((688,112,1004,583),radius=24,fill='white')
        d.text((720,130),'THERMOSTAT · DEMO',font=self.small,fill='#72819a')
        d.text((720,156),f"Room {thermostat['room_temperature_f']:.1f}° · {thermostat['hvac_state']}",font=self.small,fill='#657895')
        dial_color={'Heating':'#78452a','Cooling':'#263c69'}.get(thermostat['hvac_state'],'#253047')
        d.ellipse((731,177,961,407),fill=dial_color,outline='#cdd4df',width=7)
        d.arc((741,187,951,397),195,345,fill='#87aafa',width=5)
        d.text((798,233),str(target)+'°',font=ImageFont.truetype(FONT_PATH,65),fill='white')
        d.text((798,315),'Target °F',font=self.small,fill='#b8c8e2')
        for box,text in [((720,381,815,439),'−'),((877,381,976,439),'+')]:
            d.rounded_rectangle(box,radius=16,fill='#e9effb')
            d.text((box[0]+33,box[1]+1),text,font=self.title,fill='#325bba')
        d.rounded_rectangle((714,454,980,496),radius=13,fill='#f1f4f9')
        d.text((785,465),mode+'  ›',font=self.small,fill='#496384')
        d.text((737,510),'HEADPHONE VOLUME',font=self.small,fill='#72819a')
        for box,label in [((720,539,782,581),'−'),((912,539,976,581),'+')]:
            d.rounded_rectangle(box,radius=12,fill='#e9effb')
            d.text((box[0]+19,box[1]-1),label,font=self.title,fill='#325bba')
        d.rounded_rectangle((790,539,904,581),radius=12,fill='#fff0cb' if muted else '#e9effb')
        d.text((805,550),'Muted' if muted else str(volume)+'%',font=self.font,fill='#325bba')
        if waiting:
            d.rounded_rectangle((24,468,658,513),radius=12,fill='#fff0cb')
            d.text((40,480),'Open Muse on your phone to review the pending request.',font=self.small,fill='#855712')
        recording=voice=='recording'
        if recording:
            d.rounded_rectangle((40,511,642,519),radius=4,fill='#c8d2e6')
            if mic_level>0:d.rounded_rectangle((40,511,40+max(3,int(602*min(1,mic_level*3))),519),radius=4,fill='#31b68b')
        d.rounded_rectangle((24,524,452,583),radius=20,fill='#dc5764' if recording else '#2656bd')
        label={'idle':'Tap to talk to Muse','recording':'Listening… auto send','sending':'Sending voice…','error':'Voice error — tap to retry'}.get(voice,'Tap to talk to Muse')
        d.ellipse((47,539,69,560),fill='white')
        d.line((58,561,58,568),fill='white',width=3)
        d.text((91,538),label,font=self.font,fill='white')
        d.rounded_rectangle((474,524,658,583),radius=18,fill='#e8dce5')
        d.text((491,543),'■ Stop speech',font=self.font,fill='#8a3150')
        return img.transpose(Image.Transpose.ROTATE_180)

    def _write(self, image):
        raw=image.convert('RGBA').tobytes('raw','BGRA')
        scroll=int(self.scroll_offset)-self.written_scroll
        if not self.writer.write(raw,scroll=scroll if abs(scroll)<50 and self.chat_image is None else 0):return
        self.written_scroll=int(self.scroll_offset)
        self.frames+=1
        if self.frames==1:
            log.info('Muse logo and chat view written to board framebuffer')
        now=time.monotonic()
        if now-self.preview_saved>2:
            image.save('/tmp/ma35-muse-chat-preview.png')
            self.preview_saved=now

    def _render_loop(self):
        while not self.closed.is_set():
            self.dirty.wait(0.04)
            self.dirty.clear()
            try:
                self._write(self.render())
            except Exception as exc:
                log.warning('board display write failed: %s',exc)
                self.dirty.set()
                self.closed.wait(1)
            self.closed.wait(0.01)

    def close(self):
        self.closed.set()
        self.speaker.close()
        self.dirty.set()
        self.thread.join(timeout=9)
        self.writer.close()
