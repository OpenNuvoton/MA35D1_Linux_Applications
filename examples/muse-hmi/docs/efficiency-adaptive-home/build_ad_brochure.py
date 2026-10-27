# SPDX-License-Identifier: Apache-2.0
"""Render the adaptive-home paper as a branded architecture prospectus.

Figures are crops of real MA35 framebuffer captures. Prose preserves the
distinction between demonstrated UI, advice, estimates and future adapters.
"""
from pathlib import Path
import io, json
import fitz
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
CAPS=OUT/'board-captures'
PDF=OUT/'A_New_Dawn_for_Smart_Home_AI.pdf'
W,H=960,600
NAVY='#10182e'; INK='#19253b'; MUTED='#607087'; TEAL='#03bda9'; PURPLE='#6a54e8'; PALE='#eef3f8'; LINE='#dae3ed'
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
BOLD='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
doc=fitz.open()
def color(s):return tuple(int(s.lstrip('#')[i:i+2],16)/255 for i in (0,2,4))
def rect(p,r,fill,stroke=None,width=1):p.draw_rect(fitz.Rect(r),fill=color(fill),color=color(stroke) if stroke else None,width=width)
def text(p,r,s,size=13,bold=False,tone=INK,align=0):
    result=p.insert_textbox(fitz.Rect(r),s,fontname='DB' if bold else 'D',fontsize=size,color=color(tone),lineheight=1.25,align=align)
    if result<-.05:raise RuntimeError(f'Text overflow on page {len(doc)}: {s[:65]} ({result:.2f})')
def logo(p,x=718,y=27,dark=False):
    if dark:rect(p,(x-12,y-8,x+186,y+29),'#ffffff')
    p.insert_image(fitz.Rect(x,y,x+113,y+15),filename=str(ROOT/'native-media/assets/winbond-logo.png'))
    p.insert_image(fitz.Rect(x+123,y,x+184,y+15),filename=str(ROOT/'native-media/assets/nuvoton-logo.png'))
def page(kicker,title=None,dark=False):
    p=doc.new_page(width=W,height=H);p.insert_font(fontname='D',fontfile=FONT);p.insert_font(fontname='DB',fontfile=BOLD)
    rect(p,p.rect,NAVY if dark else '#ffffff');rect(p,(0,0,W,5),TEAL)
    text(p,(48,27,630,46),kicker.upper(),9,True,TEAL if dark else PURPLE)
    logo(p,dark=dark)
    if title:text(p,(48,76,912,156),title,35,True,'#ffffff' if dark else INK)
    p.draw_line((48,563),(912,563),color=color('#304059' if dark else LINE),width=.7)
    text(p,(48,574,860,590),'WINUVO ADAPTIVE LIVING  /  MA35D1 PROTOTYPE  /  12 OCTOBER 2026',7.6,tone='#b7c4d8' if dark else MUTED)
    text(p,(865,570,912,590),f'{len(doc):02}',10,True,TEAL,align=2)
    return p
def crop(key,box=None):
    im=Image.open(CAPS/f'{key}.png').convert('RGB')
    if box and isinstance(box[0],tuple):
        parts=[im.crop(b) for b in box];width=max(p.width for p in parts)
        parts=[p.resize((width,round(p.height*width/p.width)),Image.Resampling.LANCZOS) for p in parts]
        im=Image.new('RGB',(width,sum(p.height for p in parts)+12*(len(parts)-1)),(16,23,49))
        y=0
        for part in parts:im.paste(part,(0,y));y+=part.height+12
    elif box:im=im.crop(box)
    stream=io.BytesIO();im.save(stream,format='PNG');return stream.getvalue()
def figure(p,key,r,box=None):
    rect(p,r,'#101731');p.insert_image(fitz.Rect(r),stream=crop(key,box),keep_proportion=True)
def label(p,r,s):text(p,r,s,8.5,True,MUTED)
def paragraph(p,r,s):text(p,r,s,13,tone=MUTED)
def badge(p,x,y,w,title,detail,dark=False):
    rect(p,(x,y,x+w,y+72),'#1d2d49' if dark else PALE)
    text(p,(x+15,y+12,x+w-15,y+35),title,13,True,TEAL if dark else PURPLE)
    text(p,(x+15,y+37,x+w-15,y+68),detail,10,tone='#d4dfef' if dark else MUTED)

# Cover: clear hierarchy, co-branding and a large, real product image.
p=page('A shared-context architecture prospectus',dark=True)
text(p,(48,92,900,126),'A NEW DAWN FOR',20,True,TEAL)
text(p,(48,127,490,296),'SMART\nHOME AI',58,True,'#ffffff')
text(p,(48,303,410,369),'The Efficiency-Adaptive Home',23,True,'#e4ebf6')
paragraph_color='#b7c6df'
text(p,(48,382,400,467),'One place. One context.\nAn experience that follows the day.',16,tone=paragraph_color)
figure(p,'home',(434,164,920,450))
text(p,(449,464,912,495),'LIVE MA35 BOARD • HOME + MEDIA + FIVE-BAND EQ',8.5,True,TEAL)
text(p,(48,509,900,554),'A HOME IN SYNC WITH YOU.',25,True,'#ffffff')

p=page('01 / The central idea','One change. A whole home in sync.')
paragraph(p,(48,148,450,249),'Tell Muse a new zip code. The home resolves its place and time zone, then refreshes local weather, regional news, air estimates and every dependent recommendation against one versioned context.')
paragraph(p,(489,148,912,249),'Local date, season, daylight and time of day connect the panels. Home turns them into a daily briefing. Sources, timestamps and missing inputs stay visible throughout the experience.')
figure(p,'weather',(48,266,469,477),(216,16,1006,534))
figure(p,'news',(491,266,912,477),(216,16,1006,534))
label(p,(48,488,467,512),'WEATHER / LOCAL FORECAST + SOURCE + ZIP')
label(p,(491,488,912,512),'NEWS / REGIONAL STORIES + PUBLICATION CONTEXT')
text(p,(48,525,912,552),'95131 → resolve place → fetch sources → evaluate → publish one revision',12,True,PURPLE)

p=page('02 / Comfort, light and energy','Let the day set the rhythm.')
paragraph(p,(48,148,912,207),'Weather, date, season and daylight are shared inputs. The framework uses them together to explain comfort choices, warm the evening lighting plan and coordinate whole-home energy guidance.')
items=[('climate','COMFORT','A cooler night can support ventilation advice. Indoor readings and a thermostat adapter are the next integration.'),('lighting','LIGHTING','Local sunset anchors the transition from daylight to warmer evening scenes, with household preferences retained.'),('energy','ENERGY','Climate and lighting contribute to one strategy. Meter, tariff and measured savings are not connected in this prototype.')]
for i,(key,title,body) in enumerate(items):
    x=48+i*294;figure(p,key,(x,225,x+276,423),((216,118,713,251),(216,260,689,457)))
    text(p,(x,436,x+276,460),title,13,True,PURPLE)
    text(p,(x,469,x+276,546),body,11.1,tone=MUTED)

p=page('03 / Evidence and connectivity','A clear view of what the home knows.')
figure(p,'air',(48,159,469,398),(216,16,1006,534))
figure(p,'security',(491,159,912,398),(216,16,1006,534))
text(p,(48,415,468,443),'AIR QUALITY WITH PROVENANCE',15,True,PURPLE)
paragraph(p,(48,451,468,545),'The captured AQI and PM2.5 values are model estimates for outdoor conditions. A future purifier adapter can use fresh inputs and bounded on/off thresholds; no purifier is connected here.')
text(p,(491,415,912,443),'SECURITY THAT SAYS WHAT IS MISSING',15,True,PURPLE)
paragraph(p,(491,451,912,545),'Motion, cameras, people and doors belong in one view. This capture correctly says the feeds are unconnected. Face identification requires enrollment; physical access needs a separate authenticated decision.')

p=page('04 / The daily surface','Your day. Remixed.',dark=True)
text(p,(48,151,339,273),'Home brings the specialist panels together with favorite media: family slideshows, local video, radio, Muse answers and picture-rich PDF reports.',16,tone='#c7d5e9')
figure(p,'home',(368,153,912,472),(216,16,1008,590))
for i,(title,detail) in enumerate([('PHOTOS','Family slideshows'),('VIDEO','Favorite local clips'),('RADIO','Chosen music')]):
    badge(p,48,296+i*79,287,title,detail,True)
text(p,(381,487,901,538),'Larger media on the left. Real five-band EQ and volume on the right.',14,True,'#ffffff')

p=page('05 / Routines and listening','Make the routine yours.')
figure(p,'scenes',(48,159,469,379),(216,16,1006,534))
figure(p,'sound',(491,159,912,379),(216,16,1006,534))
text(p,(48,399,469,429),'SCENES / THE EVENING, REMIXED',15,True,PURPLE)
paragraph(p,(48,439,469,538),'A routine combines forecast, lighting and comfort context. It remains a proposal until a resident or an authorized rule executes it. Missing feed items remain visibly unsupplied.')
text(p,(491,399,912,429),'SOUND LAB / MUSE ASKS, YOU CHOOSE',15,True,PURPLE)
paragraph(p,(491,439,912,538),'Compare generated tones and choose the preferred sound. Five NAU8822 hardware EQ bands tune the listening profile. Keep the result or restore the starting sound; volume remains a separate control.')

p=page('06 / The interface contract','One XML. Qt and LVGL, together.')
paragraph(p,(48,151,424,290),'A shared XML definition drives the LVGL base and a transparent Qt overlay. Coordinates, bindings and navigation line up across the two layers. Typed APIs supply current content and accepted actions.')
paragraph(p,(48,304,424,437),'The UI is part of the contract: a changed slider must immediately repaint its thumb and label, then reflect the resulting hardware state. AI content does not need low-level graphics or codec-register access.')
figure(p,'home',(466,156,912,418),(216,250,993,592))
label(p,(466,433,912,455),'DETAIL CROP / MEDIA + VERTICAL EQ + VOLUME')
for i,(title,detail) in enumerate([('XML','Composition + bindings'),('LVGL + QT','Aligned rendering layers'),('TYPED API','Bounded data + actions')]):
    badge(p,48+i*294,477,276,title,detail)

p=page('07 / The global adaptive API','One revision. Every dependent panel.')
steps=['RESOLVE','FETCH','VALIDATE','EVALUATE','PUBLISH','EXPLAIN']
for i,step in enumerate(steps):
    x=48+i*146;rect(p,(x,160,x+133,202),PURPLE if i in (0,4) else PALE)
    text(p,(x+5,171,x+128,197),step,10.5,True,'#ffffff' if i in (0,4) else INK,align=1)
operations=[('adaptive.context','Read context, sources, freshness and revision.'),('adaptive.configure','Set a validated zip code, time zone and bounded policies.'),('adaptive.update','Accept sourced weather, air and news inputs.'),('adaptive.evaluate','Return recommendations with reasons; no actuation claim.'),('adaptive.publish','Bind all panel models to the current location revision.'),('adaptive.refresh','Ask Muse to refresh the dependent templates together.'),('adaptive.report','Generate the dated, picture-rich daily PDF.')]
for i,(name,desc) in enumerate(operations):
    y=220+i*39
    if i%2==0:rect(p,(48,y,912,y+37),PALE)
    text(p,(63,y+9,340,y+34),name,11.5,True,PURPLE)
    text(p,(345,y+9,897,y+34),desc,11.5,tone=MUTED)
text(p,(48,515,912,549),'A zip change invalidates old local inputs. Late publications for the previous revision are rejected.',12,True,INK)

p=page('08 / Picture-rich reporting','A home that can show its work.',dark=True)
text(p,(48,146,912,203),'Daily reports bind the narrative and pictures to the same place, date, time zone and context revision. Delivery can include pictures and a PDF when an installed phone or messaging adapter supports attachments.',15,tone='#c7d5e9')
keys=['home','weather','news','air','climate','lighting','energy','security','scenes','sound']
for i,key in enumerate(keys):
    x=48+(i%5)*175;y=229+(i//5)*153
    figure(p,key,(x,y,x+163,y+95))
    text(p,(x,y+104,x+163,y+128),key.upper(),9,True,TEAL)
text(p,(48,537,912,558),'TEN ACTUAL BOARD CAPTURES / UI EVIDENCE, WITH ESTIMATE AND CONNECTIVITY LABELS',9,True,'#d7e4f4')

p=page('09 / The engineering brief','Build trust into the shared context.')
text(p,(48,151,462,185),'KNOWN, ESTIMATED, SOURCED, UNKNOWN',15,True,PURPLE)
paragraph(p,(48,197,462,354),'Every input retains its basis, source and timestamp. The prototype marks weather and air stale after two hours and news after one day. These are configurable demonstration defaults. An absent adapter remains “not connected”; a recommendation remains “proposed” until actual execution is confirmed.')
text(p,(498,151,912,185),'MEASURE BEFORE CLAIMING',15,True,PURPLE)
paragraph(p,(498,197,912,354),'Evaluate coherent refresh time after a zip change, rejection of stale revisions, source comprehension and touch response. Check comfort against indoor measurements, device actions against feedback, and energy use against a measured baseline and real tariff.')
rect(p,(48,375,912,478),PALE)
text(p,(65,391,895,424),'The architectural claim',15,True,INK)
text(p,(65,427,895,469),'A shared, provenance-aware context can coordinate useful home experiences through bounded operations. The MA35D1 demonstrator establishes the UI and API framework; household sensors and adapters complete the system.',12.2,tone=MUTED)
text(p,(48,498,912,550),'AI expresses intent through named operations. Credentials, updates, auditing, recovery and household consent remain product engineering responsibilities.',12,tone=MUTED)

p=page('10 / Sources and next steps','From a visible prototype to a living home.')
paragraph(p,(48,151,462,315),'Demonstrated: ten live XML-defined views, a Qt/LVGL composition, local media, PDF presentation, generated listening tones and five-band codec controls. Captured on 12 October 2026 with zip context 95131. Crops in this prospectus preserve the original display content.')
paragraph(p,(498,151,912,315),'Next integrations: indoor sensors, a thermostat and purifier adapter, lighting feedback, enrolled motion/camera/door feeds, energy meters and tariffs. Device actions must be authorized, observable and recoverable. No energy savings, connected security feeds or room measurements are claimed by these captures.')
text(p,(48,334,912,369),'SELECTED ENGINEERING REFERENCES',13,True,PURPLE)
text(p,(48,383,912,430),'NISTIR 8259A — IoT Device Cybersecurity Capability Core Baseline. A reference for identification, configuration, data protection, access, updates and cybersecurity-state awareness.',11.3,tone=MUTED)
text(p,(48,443,912,497),'Connectivity Standards Alliance — Matter 1.0 Core Specification. A potential interoperability layer for compatible devices; this prototype is not demonstrated as Matter-connected or certified.',11.3,tone=MUTED)
p.insert_link({'kind':fitz.LINK_URI,'from':fitz.Rect(48,383,912,430),'uri':'https://csrc.nist.gov/pubs/ir/8259/a/final'})
p.insert_link({'kind':fitz.LINK_URI,'from':fitz.Rect(48,443,912,497),'uri':'https://csa-iot.org/wp-content/uploads/2022/11/22-27349-001_Matter-1.0-Core-Specification.pdf'})
text(p,(48,519,912,553),'Winuvo Adaptive Living / A New Dawn for Smart Home AI',16,True,INK)

doc.set_metadata({'title':'A New Dawn for Smart Home AI','author':'Winuvo Adaptive Living / MA35D1 prototype','subject':'The Efficiency-Adaptive Home — architecture prospectus with live board captures','keywords':'Winbond Nuvoton Winuvo MA35D1 Muse Qt LVGL adaptive home'})
doc.subset_fonts();doc.save(PDF,garbage=4,deflate=True)
preview=OUT/'previews';preview.mkdir(parents=True,exist_ok=True)
for i in [0,1,2,4,6,8]:
    doc[i].get_pixmap(matrix=fitz.Matrix(1.25,1.25)).save(preview/f'page-{i+1:02d}.png')
print(json.dumps({'pdf':str(PDF),'pages':len(doc),'bytes':PDF.stat().st_size},indent=2))
