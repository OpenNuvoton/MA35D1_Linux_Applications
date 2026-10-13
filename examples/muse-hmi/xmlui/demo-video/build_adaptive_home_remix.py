# SPDX-License-Identifier: Apache-2.0
"""Build a 30-second reel with matching narration, captions and board captures.

Run with a Python environment containing requirements-demo.txt and ffmpeg.
--voice-python optionally selects a separate Kokoro synthesis environment. --seed recreates the same original music and camera choices.
"""
from pathlib import Path
import argparse, json, math, random, subprocess, sys, wave
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'docs/efficiency-adaptive-home/reel'
CAPS = OUT.parent / 'board-captures'
FPS, RATE, SECONDS = 30, 48000, 30
PLAN = [
    ('home', 'HOME IN MOTION', 'A home that moves with you.'),
    ('weather', 'CHANGE YOUR PLACE', 'New zip code. New forecast.'),
    ('news', 'YOUR LOCAL BRIEFING', 'Local stories, right where you are.'),
    ('air', 'AIR, WITH CONTEXT', 'Air estimates guide healthier choices.'),
    ('climate', 'COMFORT IN CONTEXT', 'Comfort follows the weather.'),
    ('lighting', 'FOLLOW THE SUN', 'Lighting follows the sun.'),
    ('energy', 'ONE HOME, ONE MIX', 'Energy works with the whole home.'),
    ('security', 'READY TO CONNECT', 'Security, ready for cameras and doors.'),
    ('scenes', 'REMIX THE EVENING', 'Evening routines, remixed.'),
    ('home', 'PLAY YOUR FAVORITES', 'Photos, video, radio. Your favorites.'),
    ('sound', 'SOUND, YOUR WAY', 'Sound Lab. Tune your theater by ear.'),
    ('finale', 'A NEW DAWN', 'Winuvo. A new dawn for adaptive living.'),
]

def wav_write(path, samples, rate):
    samples = np.asarray(samples)
    channels = 1 if samples.ndim == 1 else samples.shape[1]
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(channels); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes((np.clip(samples, -.999, .999)*32767).astype('<i2').tobytes())

def synthesize(args):
    from kokoro_onnx import Kokoro
    voice = Kokoro(str(args.kokoro_model), str(args.kokoro_voices))
    meta = []
    for i, (key, title, caption) in enumerate(PLAN):
        spoken = caption.replace('Winuvo', 'Win yoo voh')
        samples, rate = voice.create(spoken, voice='af_heart', speed=1.0,
                                     lang='en-us', sentence_pause=.20,
                                     clause_pause=.08)
        wav_write(OUT/f'voice-{i:02d}.wav', samples, rate)
        meta.append(dict(screen=key, title=title, caption=caption,
                         spoken_text=spoken, voice_seconds=len(samples)/rate))
        print(f'Voice {i+1}/{len(PLAN)}: {len(samples)/rate:.2f}s', flush=True)
    (OUT/'voice-plan.json').write_text(json.dumps(meta, indent=2))

def timing(seed):
    scenes = json.loads((OUT/'voice-plan.json').read_text())
    total = sum(s['voice_seconds'] for s in scenes)
    # Keep speech natural; fit only if the chosen voice exceeds the reel budget.
    factor = max(1.0, total/26.1)
    gap = .22
    slack = SECONDS - total/factor - gap*len(scenes)
    if slack < -.001: raise RuntimeError('Narration does not fit')
    weights = [1]*len(scenes); weights[0] = 1.5; weights[-1] = 2.5
    cursor = 0; audio = np.zeros(RATE*SECONDS, dtype=np.float32)
    for i, s in enumerate(scenes):
        with wave.open(str(OUT/f'voice-{i:02d}.wav')) as w:
            rate = w.getframerate()
            samples = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(np.float32)/32768
        resampled = np.interp(np.arange(int(len(samples)*RATE/rate/factor))*rate*factor/RATE,
                              np.arange(len(samples)), samples)
        length = len(resampled)/RATE
        s.update(start=cursor, voice_start=cursor+.12,
                 voice_end=cursor+.12+length,
                 end=cursor+length+gap+slack*weights[i]/sum(weights))
        at = round(s['voice_start']*RATE)
        audio[at:at+len(resampled)] = resampled
        cursor = s['end']
    scenes[-1]['end'] = SECONDS
    wav_write(OUT/'narration.wav', audio, RATE)
    manifest = dict(seed=seed, duration=SECONDS, voice='Kokoro af_heart',
                    speech_rate_adjustment=factor, source='live MA35 captures, 12 October 2026', scenes=scenes)
    (OUT/'storyboard.json').write_text(json.dumps(manifest, indent=2))
    (OUT/'narration.txt').write_text('\n'.join(s['caption'] for s in scenes)+'\n')
    def stamp(t):
        ms=round(t*1000);return f'{ms//3600000:02}:{ms//60000%60:02}:{ms//1000%60:02},{ms%1000:03}'
    (OUT/'Winuvo_30s_Remix.srt').write_text('\n\n'.join(
        f'{i+1}\n{stamp(s["voice_start"])} --> {stamp(min(s["end"],s["voice_end"]+.10))}\n{s["caption"]}'
        for i,s in enumerate(scenes))+'\n')
    return scenes

def score(scenes, seed):
    rng=np.random.default_rng(seed); music=np.zeros((RATE*SECONDS,2),np.float32)
    def add(at, sound, pan=.5, gain=1):
        ix=max(0,round(at*RATE));n=min(len(sound),len(music)-ix)
        if n<=0:return
        music[ix:ix+n,0]+=sound[:n]*math.sqrt(1-pan)*gain
        music[ix:ix+n,1]+=sound[:n]*math.sqrt(pan)*gain
    def midi(n):return 440*2**((n-69)/12)
    roots=[50,53,48,55,50,58,53,48,55,50,57,50]
    for i,s in enumerate(scenes):
        dur=s['end']-s['start'];t=np.arange(round(dur*RATE))/RATE
        env=np.minimum(1,t/.18)*np.minimum(1,(dur-t)/.18)
        pad=np.zeros(len(t));root=roots[i]
        for note in [root,root+7,root+10,root+14]:
            f=midi(note)
            for h,a in [(1,.035),(2,.012),(3,.006),(4,.002)]:
                pad+=a*np.sin(2*np.pi*f*h*t+.10*np.sin(2*np.pi*4*t))
                pad+=a*.75*np.sin(2*np.pi*f*h*1.003*t+.6)
        add(s['start'],pad*env,pan=.36+.22*(i%2))
        # Scene-aligned impacts and short stereo risers connect the sound to cuts.
        t=np.arange(int(.65*RATE))/RATE
        hit=np.sin(2*np.pi*(53*t+2.4*(1-np.exp(-t*22))))*np.exp(-t*7)*.31
        add(s['start'],hit,.5)
        if i:
            t=np.arange(int(.28*RATE))/RATE
            sweep=rng.normal(0,1,len(t)).astype(np.float32)
            sweep=np.convolve(sweep,np.ones(13)/13,mode='same')
            add(s['start']-.28,sweep*(t/.28)**2*.15,.2 if i%2 else .8)
    beat=60/112
    for j,at in enumerate(np.arange(0,SECONDS,beat/2)):
        i=next((k for k,s in enumerate(scenes) if s['start']<=at<s['end']),len(scenes)-1)
        t=np.arange(int(.34*RATE))/RATE
        note=roots[i]+int(rng.choice([12,15,19,22,24]))
        pluck=(np.sin(2*np.pi*midi(note)*t)+.20*np.sin(4*np.pi*midi(note)*t))
        add(at,pluck*np.exp(-t*14)*np.minimum(1,t/.004)*.10,.15+.70*float(rng.random()))
        if j%2==0:
            kick=np.sin(2*np.pi*(46*t+1.7*(1-np.exp(-t*30))))*np.exp(-t*15)*.23
            add(at,kick,.5)
        hat=rng.normal(0,1,len(t)).astype(np.float32)
        hat=hat-np.convolve(hat,np.ones(7)/7,mode='same')
        add(at,hat*np.exp(-t*100)*(.018 if j%4 else .032),.3+.4*(j%2))
        if j%4==2:
            snare=rng.normal(0,1,len(t)).astype(np.float32)*np.exp(-t*28)*.075
            add(at,snare,.5)
    music/=max(1e-6,np.max(np.abs(music)));music*=.66
    t=np.arange(len(music))/RATE
    music*=np.clip(np.minimum(t/.55,(SECONDS-t)/1.1),0,1)[:,None]
    wav_write(OUT/'original-remix-score.wav',music,RATE)

def render(scenes,seed):
    from PIL import Image, ImageDraw, ImageFont, ImageFilter
    W,H=1920,1080;rand=random.Random(seed)
    font='/usr/share/fonts/truetype/dejavu/DejaVuSans'
    def f(n,bold=False):return ImageFont.truetype(font+('-Bold' if bold else '')+'.ttf',n)
    fonts={n:f(n,b) for n,b in [(20,False),(26,False),(34,True),(42,True),(48,True),(96,True),(130,True)]}
    images={p.stem:Image.open(p).convert('RGB') for p in CAPS.glob('*.png')}
    palette=[(52,238,215),(134,112,255),(249,184,79),(74,214,250)]
    y,x=np.mgrid[0:H,0:W];backgrounds=[]
    for c in palette:
        glow=np.exp(-((x-1400)**2/900000+(y-240)**2/500000))
        bg=np.zeros((H,W,3),np.uint8)
        for k,base in enumerate((7,12,29)):bg[:,:,k]=np.clip(base+glow*c[k]*.13,0,255)
        backgrounds.append(Image.fromarray(bg))
    choices=[(rand.choice(['slide','zoom','glitch','rise']),rand.choice([-1,1]),rand.uniform(.02,.05)) for _ in scenes]
    particles=[(rand.randrange(W),rand.randrange(H),rand.uniform(6,22),rand.uniform(1,3)) for _ in range(42)]
    logo_images=[Image.open(ROOT/'native-media/assets'/n).convert('RGBA') for n in ['winbond-logo.png','nuvoton-logo.png']]
    def logo(frame,ypos):
        draw=ImageDraw.Draw(frame);draw.rounded_rectangle((694,ypos-10,1226,ypos+69),18,fill='white')
        xx=720
        for im,ww in zip(logo_images,[285,155]):
            hh=round(im.height*ww/im.width);tile=im.resize((ww,hh),Image.Resampling.LANCZOS)
            frame.paste(tile,(xx,ypos+(48-hh)//2),tile);xx+=ww+32
    def text_center(draw,txt,ypos,size,color):
        fo=fonts[size];ww=draw.textlength(txt,font=fo);draw.text(((W-ww)/2,ypos),txt,font=fo,fill=color)
    previews=[];preview_frames={round((s['start']+s['end'])/2*FPS):i for i,s in enumerate(scenes)}
    proc=subprocess.Popen(['ffmpeg','-v','error','-y','-f','rawvideo','-pix_fmt','rgb24','-s','1920x1080','-r',str(FPS),'-i','pipe:0','-an','-c:v','libx264','-preset','fast','-crf','18','-pix_fmt','yuv420p',str(OUT/'picture.mp4')],stdin=subprocess.PIPE)
    current=0
    try:
        for frame_n in range(FPS*SECONDS):
            t=frame_n/FPS
            while current+1<len(scenes) and t>=scenes[current]['end']:current+=1
            s=scenes[current];local=t-s['start'];dur=s['end']-s['start'];p=local/dur
            accent=palette[current%len(palette)];frame=backgrounds[current%len(palette)].copy();draw=ImageDraw.Draw(frame)
            for xx,yy,v,r in particles:
                yy=(yy-t*v)%H;draw.ellipse((xx-r,yy-r,xx+r,yy+r),fill=accent+(255,))
            for k in range(6):
                xx=int((t*35+k*350)%2400)-240
                draw.line((xx,H,xx+500,0),fill=tuple(int(a*.12) for a in accent),width=1)
            if s['screen']=='finale':
                # Only the finale remixes multiple screens; feature narration
                # elsewhere always stays on its own matching board screen.
                for j,key in enumerate(['weather','news','lighting','energy','sound','home']):
                    tile=images[key].resize((600,352),Image.Resampling.LANCZOS)
                    tx=-20+(j%3)*670+int(14*math.sin(t+j));ty=70+(j//3)*390
                    frame.paste(tile,(tx,ty))
                frame=Image.blend(frame,Image.new('RGB',(W,H),(5,10,24)),.75);draw=ImageDraw.Draw(frame)
                text_center(draw,'WINUVO',235,130,'white')
                text_center(draw,'A NEW DAWN',415,96,accent)
                text_center(draw,'FOR ADAPTIVE LIVING',540,48,'white')
                logo(frame,660);draw=ImageDraw.Draw(frame)
            else:
                im=images[s['screen']];effect,direction,zmax=choices[current]
                # A fast focus cut highlights details without changing topics.
                punch=.12*math.exp(-((p-.57)/.085)**2)
                zoom=1+zmax*p+.11*(3*p*p-2*p*p*p)+punch
                cw,ch=im.width/zoom,im.height/zoom
                cx,cy=(.54+.045*math.sin(p*math.pi))*im.width,.47*im.height
                left=min(im.width-cw,max(0,cx-cw/2));top=min(im.height-ch,max(0,cy-ch/2))
                tile=im.crop((left,top,left+cw,top+ch)).resize((1372,804),Image.Resampling.LANCZOS)
                intro=max(0,1-local/.19)
                xx=274+int(22*math.sin(p*2.8)+direction*intro*350 if effect=='slide' else 22*math.sin(p*2.8))
                yy=126+int(5*math.sin(p*4)+(intro*160 if effect=='rise' else 0))
                if effect=='zoom' and intro:
                    tile=tile.resize((int(1372*(1+.15*intro)),int(804*(1+.15*intro))),Image.Resampling.BICUBIC)
                draw.rounded_rectangle((xx-8,yy-8,xx+tile.width+8,yy+tile.height+8),16,fill=accent)
                frame.paste(tile,(xx,yy));draw=ImageDraw.Draw(frame)
                if effect=='glitch' and intro:
                    for row in [1,3,5]:
                        strip=frame.crop((xx,yy+row*110,xx+1372,yy+row*110+28))
                        frame.paste(strip,(xx+int(direction*intro*50),yy+row*110))
                draw.text((64,48),'WINUVO',font=fonts[42],fill='white')
                draw.text((65,100),'ADAPTIVE LIVING',font=fonts[20],fill=accent)
                tw=draw.textlength(s['title'],font=fonts[34]);draw.text((W-70-tw,58),s['title'],font=fonts[34],fill='white')
                draw.text((58,265),f'{current+1:02d}',font=fonts[96],fill=tuple(int(a*.35) for a in accent))
                for k in range(8):
                    h=14+int(30*(.5+.5*math.sin(t*8+k*1.9)))
                    draw.rounded_rectangle((65+k*12,625-h,71+k*12,625),3,fill=accent)
            draw=ImageDraw.Draw(frame)
            if s['voice_start']<=t<=min(s['end'],s['voice_end']+.1):
                caption=s['caption'];size=42 if len(caption)<48 else 34
                ww=draw.textlength(caption,font=fonts[size]);bx=(W-ww)/2-35
                draw.rounded_rectangle((bx,961,W-bx,1042),20,fill=(6,12,28),outline=accent,width=2)
                text_center(draw,caption,978,size,'white')
            draw.rectangle((0,H-7,int(W*t/SECONDS),H),fill=accent)
            if local<.10 and current:
                frame=Image.blend(frame,Image.new('RGB',(W,H),accent),.12*(1-local/.10))
            if t>29.55:frame=Image.blend(frame,Image.new('RGB',(W,H),(4,8,20)),min(1,(t-29.55)/.45))
            if frame_n in preview_frames:
                thumb=frame.copy();thumb.thumbnail((480,270));previews.append((preview_frames[frame_n],thumb))
            proc.stdin.write(frame.tobytes())
            if frame_n%90==0:print(f'Edit {t:.0f}/30s: {s["screen"]}',flush=True)
        proc.stdin.close()
        if proc.wait():raise RuntimeError('Video encoder failed')
    except BaseException:
        proc.kill();raise
    sheet=Image.new('RGB',(1440,4*310),(9,15,31));d=ImageDraw.Draw(sheet)
    for i,thumb in previews:
        at=(i%3*480,i//3*310);sheet.paste(thumb,at)
        d.text((at[0]+14,at[1]+276),scenes[i]['screen'].upper(),font=f(17,True),fill='white')
    sheet.save(OUT/'contact-sheet.png')

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--voice-only',action='store_true');ap.add_argument('--seed',type=int)
    ap.add_argument('--kokoro-model',type=Path,default=ROOT/'xmlui/vendor/tts/kokoro-v1.0.int8.onnx')
    ap.add_argument('--kokoro-voices',type=Path,default=ROOT/'xmlui/vendor/tts/voices-v1.0.bin')
    ap.add_argument('--voice-python',default=sys.executable)
    args=ap.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    if args.voice_only:synthesize(args);return
    seed=args.seed if args.seed is not None else random.SystemRandom().randrange(2**31)
    subprocess.run([args.voice_python,str(Path(__file__).resolve()),'--voice-only','--kokoro-model',str(args.kokoro_model.resolve()),'--kokoro-voices',str(args.kokoro_voices.resolve())],check=True)
    scenes=timing(seed);score(scenes,seed);render(scenes,seed)
    subprocess.run(['ffmpeg','-v','error','-y','-i',str(OUT/'narration.wav'),'-af','loudnorm=I=-16:TP=-2:LRA=8','-ar','48000','-ac','2',str(OUT/'narration-master.wav')],check=True)
    mix='[1:a]apad=whole_dur=30,asplit=2[voice][key];[2:a]volume=.55[m];[m][key]sidechaincompress=threshold=.025:ratio=5:attack=15:release=220[duck];[duck][voice]amix=inputs=2:duration=first:normalize=0,alimiter=limit=.93:level=0[a]'
    final=OUT.parent/'Winuvo_30s_Remix.mp4'
    subprocess.run(['ffmpeg','-v','error','-y','-i',str(OUT/'picture.mp4'),'-i',str(OUT/'narration-master.wav'),'-i',str(OUT/'original-remix-score.wav'),'-filter_complex',mix,'-map','0:v:0','-map','[a]','-c:v','copy','-c:a','aac','-b:a','192k','-ar','48000','-ac','2','-t','30','-movflags','+faststart',str(final)],check=True)
    print(json.dumps({'video':str(final),'seed':seed,'scenes':len(scenes)},indent=2),flush=True)

if __name__=='__main__':main()
