"""Fetch a bounded HTTPS image on the gateway and display it on the board."""
import re
import io
import urllib.request
from urllib.parse import urlsplit
from PIL import Image,ImageOps
from musegadget.executor import ok,error

IMAGE_SPEC={
 'description':'Show a photograph or generated image on the MA35 board screen. Pass a directly downloadable HTTPS image URL, not a webpage. For generated images use the generated image download URL. The gateway decodes and resizes it; the board receives pixels. Use clear=true to return to chat.',
 'required':{},
 'optional':{'image_url':{'type':'string','description':'Direct HTTPS URL to PNG, JPEG or WebP image.'},'caption':{'type':'string','description':'Short caption or source attribution.'},'clear':{'type':'boolean','description':'Clear the displayed image and return to chat.'}},
 'timeout_ms':30000,
}
MAX_BYTES=8*1024*1024

def decode_image(data):
 if len(data)>MAX_BYTES:raise ValueError('image exceeds 8 MB')
 with Image.open(io.BytesIO(data)) as source:
  if source.width*source.height>16000000:raise ValueError('image resolution too large')
  source=ImageOps.exif_transpose(source)
  return ImageOps.contain(source.convert('RGB'),(634,348))

def show_image(display,params):
 if display is None:return error('display is not running')
 if not isinstance(params,dict) or set(params)-set(IMAGE_SPEC['optional']):return error('invalid image parameters')
 if 'clear' in params and type(params['clear']) is not bool:return error('clear must be boolean')
 if params.get('clear'):
  with display.lock:display.chat_image=None
  display.dirty.set();return ok({'displayed':False})
 url=params.get('image_url');caption=params.get('caption','')
 if not isinstance(url,str) or urlsplit(url).scheme!='https' or not urlsplit(url).hostname:return error('a direct HTTPS image_url is required')
 if not isinstance(caption,str) or len(caption)>180:return error('caption must be text of at most 180 characters')
 try:
  request=urllib.request.Request(url,headers={'User-Agent':'Muse-MA35-image-display/1.0'})
  with urllib.request.urlopen(request,timeout=15) as response:
   if urlsplit(response.geturl()).scheme!='https':raise ValueError('HTTPS image required')
   data=response.read(MAX_BYTES+1)
  image=decode_image(data)
  with display.lock:display.chat_image=image;display.image_caption=caption
  display.dirty.set()
  return ok({'displayed':True,'width':image.width,'height':image.height})
 except Exception as exc:
  # URLs may be signed; avoid echoing them in errors or logs.
  return error('image download or decoding failed: '+type(exc).__name__)


def image_url_from_text(text):
 if not isinstance(text,str):return None
 for candidate in re.findall(r'https://[^\s<>"\)]+',text):
  candidate=candidate.rstrip('.,')
  if urlsplit(candidate).path.lower().endswith(('.png','.jpg','.jpeg','.webp')):
   return candidate
 return None


def completed_image_url(display,event):
 text=event.get('text','')
 if not text:
  with display.lock:
   message=display.messages.get(event.get('message_id'),{})
   if message.get('role')=='Muse':text=message.get('text','')
 return image_url_from_text(text)


def collect_image_urls(value):
 urls=[]
 def visit(item,depth=0):
  if depth>12 or len(urls)>=8:return
  if isinstance(item,str):
   url=image_url_from_text(item)
   if url and url not in urls:urls.append(url)
  elif isinstance(item,dict):
   mime=item.get('mime_type',item.get('content_type',''))
   is_image=isinstance(mime,str) and mime.startswith('image/') or item.get('type')=='image'
   if is_image:
    for key in ('url','uri','download_url','image_url','src'):
     url=item.get(key)
     if isinstance(url,str) and url.startswith('https://') and url not in urls:urls.append(url)
   for child in item.values():visit(child,depth+1)
  elif isinstance(item,list):
   for child in item[:64]:visit(child,depth+1)
 visit(value)
 return urls[:8]
