"""Tests the actual C++ API's parsing, limits and view controls."""
import json
import subprocess
import os
import socket
import time
from pathlib import Path
import pytest

@pytest.fixture(scope='module')
def binary(tmp_path_factory):
 root=Path(__file__).parent
 binary=tmp_path_factory.mktemp('native-media')/'service'
 subprocess.run(['g++','-std=c++17','-O0','-pthread',str(root/'media_service.cpp'),'-o',str(binary)],check=True)
 return binary

@pytest.mark.parametrize('params,kind',[
 ({'url':'https://site/x.PNG?token=secret'},'image'),
 ({'url':'https://site/x.pdf'},'pdf'),
 ({'url':'https://site/live.m3u8'},'video'),
 ({'url':'https://site/song.opus'},'audio'),
 ({'url':'https://youtu.be/MT-4Bk1Lw8g?si=abc'},'youtube'),
 ({'url':'https://site/animation.gif'},'animation'),
 ({'text':'hello'},'text'),
 ({'kind':'pdf','url':'https://site/download'},'pdf'),
 ({'url':'https://site/download'},'unknown'),
])
def test_classify(binary,params,kind):
 assert subprocess.check_output([str(binary),'--classify',json.dumps(params)],text=True).strip()==kind

def test_view_controls_and_invalid_zoom(binary):
 commands=[{'action':'media.control','params':{'operation':'zoom','value':2}},
 {'action':'media.control','params':{'operation':'pan','x':-120,'y':-240}},
 {'action':'media.control','params':{'operation':'fullscreen','enabled':True}},
 {'action':'media.control','params':{'operation':'page','value':3}},
 {'action':'media.status'},
 {'action':'media.control','params':{'operation':'zoom','value':99}},
 {'action':'media.control','params':{'operation':'fit'}},
 {'action':'media.status'}]
 results=json.loads(subprocess.check_output([str(binary),'--batch',json.dumps(commands)],text=True))
 assert results[4]['payload']['zoom']==2
 assert results[4]['payload']['fullscreen'] is True
 assert results[4]['payload']['page']==3
 assert results[5]['ok'] is False
 assert results[7]['payload']['zoom']==1

def test_capabilities_do_not_claim_pinch_or_drm(binary):
 r=json.loads(subprocess.check_output([str(binary),'--request',json.dumps({'action':'media.capabilities'})],text=True))
 assert r['payload']['pinch'] is False
 assert r['payload']['runtime']=='board-native'
 assert r['payload']['limits']['drm'] is False


def test_queue_is_owned_by_board_service(binary):
 commands=[{'action':'media.enqueue','params':{'kind':'pdf','url':'/opt/muse-media/assets/FSA506.pdf'}},{'action':'media.status'}]
 result=json.loads(subprocess.check_output([str(binary),'--batch',json.dumps(commands)],text=True))
 assert result[0]['payload']['queued']==1
 assert result[1]['payload']['queued']==1

@pytest.mark.parametrize('start,release,full,expected',[
 ((960,570),(800,505),False,(960,570,False)),
 ((750,435),(500,340),False,(750,435,False)),
 ((500,555),(400,500),False,(500,555,False)),
 ((950,25),(850,60),True,(950,25,False)),
 ((940,83),(955,98),True,(940,83,False)),
 ((100,108),(180,150),False,(100,108,False)),
 ((100,66),(140,135),False,(100,66,False)),
 ((800,80),(820,140),False,(800,80,False)),
 ((1003,119),(1003,140),False,(1003,119,False)),
 ((317,128),(305,145),False,(317,128,False)),
 ((672,133),(659,143),False,(672,133,False)),
 ((300,250),(420,300),False,(420,300,True)),
])
def test_button_jitter_does_not_turn_buttons_into_drag(binary,start,release,full,expected):
 params={'start_x':start[0],'start_y':start[1],'x':release[0],'y':release[1],'drag':True,'fullscreen':full}
 actual=json.loads(subprocess.check_output([str(binary),'--touch-target',json.dumps(params)],text=True))
 assert (actual['x'],actual['y'],actual['drag'])==expected


def test_tone_requests_are_relative_and_default_restores_flat(binary):
 def tone(preset, **extra):
  return {'action':'audio.tune','params':{'preset':preset,**extra}}
 commands=[tone('bass_up'),tone('bass_up'),tone('treble_down',amount_db=2),
           {'action':'media.control','params':{'operation':'dsp','feature':'software_eq','enabled':False}},
           tone('flat'),tone('bass_up',amount_db=99),tone('unknown'),
           {'action':'media.control','params':{'operation':'equalizer','band':10,'value':0}}]
 results=json.loads(subprocess.check_output([str(binary),'--batch',json.dumps(commands)],text=True))
 assert results[0]['payload']['equalizer_db'][:3]==[3,3,3]
 assert results[1]['payload']['equalizer_db'][:3]==[6,6,6]
 assert results[2]['payload']['equalizer_db'][7:]==[-2,-2,-2]
 assert results[4]['payload']['equalizer_db']==[0]*10
 assert results[4]['payload']['software_eq_enabled'] is True
 assert results[4]['payload']['hardware_eq_enabled'] is False
 assert all(not result['ok'] for result in results[5:])


def test_hardware_controls_report_missing_codec(binary):
 commands=[{'action':'media.control','params':{'operation':'dsp','feature':'limiter','enabled':True}},
           {'action':'media.control','params':{'operation':'hardware_equalizer','band':0,'value':4}}]
 results=json.loads(subprocess.check_output([str(binary),'--batch',json.dumps(commands)],text=True))
 assert all(not result['ok'] and 'unavailable' in result['error'] for result in results)


def test_hot_tabs_restore_content_views_and_survive_restart(binary, tmp_path):
 """Exercise the renderer and real HTTP API, including an offline cached image."""
 from PIL import Image
 fb=tmp_path/'fb';fb.write_bytes(bytes(1024*600*4))
 assets=tmp_path/'assets';assets.mkdir()
 Image.new('RGB',(100,100),'red').save(assets/'photo.png')
 # The host renderer permits the same board asset prefix; a tiny curl shim
 # serves the initial download then is removed to prove the cache is used.
 curl=tmp_path/'curl'
 curl.write_text('#!/bin/sh\nwhile [ "$1" != "-o" ]; do shift; done\ncp "'+str(assets/'photo.png')+'" "$2"\n')
 curl.chmod(0o755)
 env={**os.environ,'MUSE_MEDIA_BASE':str(tmp_path)+'/', 'MUSE_MEDIA_CACHE_DIR':str(tmp_path/'cache')}
 def start():
  proc=subprocess.Popen([str(binary),str(fb)],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  for _ in range(100):
   if proc.poll() is not None:pytest.fail('media service failed to start')
   try:
    with socket.create_connection(('127.0.0.1',8765),timeout=.1):return proc
   except OSError:time.sleep(.02)
  proc.terminate();pytest.fail('media API did not start')
 def api(action,params=None):
  body=json.dumps({'action':action,'params':params or {}}).encode()
  with socket.create_connection(('127.0.0.1',8765),timeout=3) as client:
   client.sendall(b'POST /media HTTP/1.1\r\nContent-Length: '+str(len(body)).encode()+b'\r\n\r\n'+body)
   data=b''
   while chunk:=client.recv(65536):data+=chunk
  return json.loads(data.split(b'\r\n\r\n',1)[1])
 def ready():
  for _ in range(100):
   status=api('media.status')['payload']
   if status['state']=='error':pytest.fail(status['error'])
   if status['frames']>0:return status
   time.sleep(.02)
  pytest.fail('renderer did not present')
 proc=start()
 try:
  assert api('media.switch',{'kind':'pdf'})['ok'];assert ready()['media_kind']=='pdf'
  assert api('media.show',{'kind':'text','text':'Cached text'})['ok'];ready()
  # Report real readiness; a failed source must never become 100 percent.
  progress=api('media.progress',{'operation':'begin','topic':'Yankees'})['payload']
  generation=progress['generation'];assert progress['visible'] and progress['percent']==0
  assert api('media.progress',{'operation':'activity','detail':'Muse acknowledged request (HTTP 200)'})['payload']['percent']==0
  activity=api('media.progress',{'operation':'activity','detail':'Muse: Searching images'})['payload']
  assert activity['activity'][-2:]==['Muse acknowledged request (HTTP 200)','Muse: Searching images']
  assert not api('media.progress',{'operation':'activity','generation':generation-1,'detail':'Old request'})['ok']
  for index in [0,1,2]:api('media.progress',{'operation':'update','generation':generation,'index':index,'state':2})
  assert api('media.progress',{'operation':'update','generation':generation,'index':3,'state':3})['payload']['percent']==75
  assert not api('media.progress',{'operation':'update','generation':generation-1,'index':3,'state':2})['ok']
  assert api('media.progress',{'operation':'update','generation':generation,'index':3,'state':4})['payload']['percent']==100
  api('media.progress',{'operation':'dismiss'});assert not api('media.status')['payload']['readiness']['visible']

  api('media.control',{'operation':'zoom','value':2})
  api('media.control',{'operation':'pan','x':-15,'y':-20})
  assert api('media.show',{'kind':'image','url':'https://example.test/photo.png'})['ok'];ready()
  first=fb.read_bytes();curl.unlink()
  assert api('media.switch',{'kind':'text'})['ok']
  status=ready();assert status['zoom']==2 and status['media_kind']=='text'
  assert api('media.switch',{'kind':'image'})['ok'];ready()
  assert fb.read_bytes()==first
  cached_image=Path(json.loads((tmp_path/'cache'/'tabs.json').read_text())[1]['_cache_file'])
  assert Path(str(cached_image)+'.rgb').is_file()
  cached_image.unlink()  # Decoded pixels alone must survive an offline revisit.
  assert api('media.switch',{'kind':'text'})['ok'];ready()
  assert api('media.switch',{'kind':'image'})['ok'];ready()
  assert fb.read_bytes()==first
  # Old full-resolution downloads must not override the new display-sized source.
  Image.new('RGB',(5000,100),'blue').save(cached_image,format='JPEG')
  Path(str(cached_image)+'.rgb').unlink()
  curl.write_text('#!/bin/sh\nwhile [ "$1" != "-o" ]; do shift; done\ncp "'+str(assets/'photo.png')+'" "$2"\n');curl.chmod(0o755)
  assert api('media.switch',{'kind':'text'})['ok'];ready()
  assert api('media.switch',{'kind':'image'})['ok'];ready();assert fb.read_bytes()==first
  curl.unlink()
  # A decoded PDF page must also skip its backend on subsequent visits.
  report=assets/'report.pdf';report.write_text('%PDF-1.4 test backend fixture')
  mutool=tmp_path/'mutool'
  mutool.write_text('#!/bin/sh\nwhile [ "$1" != "-o" ]; do shift; done\ncp "'+str(assets/'photo.png')+'" "$2"\n');mutool.chmod(0o755)
  assert api('media.show',{'kind':'pdf','url':str(report)})['ok'];ready();pdf_frame=fb.read_bytes()
  # Finger drags must not push a fitted PDF entirely out of its viewport.
  count=api('media.status')['payload']['frames']
  api('media.control',{'operation':'pan','x':-209,'y':-1138})
  for _ in range(100):
   if api('media.status')['payload']['frames']>count:break
   time.sleep(.02)
  assert fb.read_bytes()==pdf_frame
  mutool.write_text('#!/bin/sh\nexit 49\n')
  assert api('media.switch',{'kind':'image'})['ok'];ready()
  assert api('media.switch',{'kind':'pdf'})['ok'];ready();assert fb.read_bytes()==pdf_frame
  assert api('media.switch',{'kind':'image'})['ok'];ready()
  assert [t['cached'] for t in api('media.status')['payload']['tabs']]==[True,True,True,False,False]
  assert not api('media.show',{'kind':'image','url':'https://example.test/bad.png','page':0})['ok']
  assert api('media.status')['payload']['media_kind']=='image'
  ui={'title':'Baseball stats','subtitle':'Example data only','blocks':[{'type':'bar','title':'Home runs','labels':['A','B'],'series':[{'name':'HR','values':[20,32]}]}]}
  assert api('media.ui',{'document':ui})['ok'];status=ready()
  assert status['generative_ui'];pid=status['worker_pid'];before=fb.read_bytes()
  # Speech reads visible chart values, not an assistant recap, and stop supersedes read.
  assert api('media.remember',{'kind':'text','text':'hidden assistant recap'})['ok']
  assert api('media.speech',{'operation':'read'})['ok']
  speech=api('media.speech',{'operation':'poll'})['payload']
  assert speech['request']==1 and 'Baseball stats' in speech['text'] and '20' in speech['text'] and '32' in speech['text']
  assert 'hidden assistant recap' not in speech['text']
  assert api('media.speech',{'operation':'poll'})['payload']['request']==0
  api('media.speech',{'operation':'read'});api('media.speech',{'operation':'stop'})
  assert api('media.speech',{'operation':'poll'})['payload']=={'request':2}
  assert api('media.status')['payload']['speech']['phase']==0
  assert api('media.status')['payload']['generative_ui']
  before=fb.read_bytes()
  ui['blocks'][0]['series'][0]['values']=[32,20]
  assert api('media.ui',{'document':ui})['payload']['live_update']
  for _ in range(100):
   if fb.read_bytes()!=before:break
   time.sleep(.02)
  else:pytest.fail('live chart did not redraw')
  assert api('media.status')['payload']['worker_pid']==pid
  assert api('media.remember',{'kind':'text','text':'Here is the comparison'})['ok']
  assert api('media.status')['payload']['generative_ui']
  assert api('media.switch',{'kind':'image'})['ok'];ready()
  assert api('media.switch',{'kind':'text'})['ok'];assert ready()['generative_ui']
  assert json.loads((tmp_path/'cache'/'tabs.json').read_text())[0]['ui']==ui
  assert api('media.switch',{'kind':'image'})['ok'];ready()
  # A later Muse refinement can rename the title while retaining sourced media.
  original_slots=json.loads((tmp_path/'cache'/'tabs.json').read_text())
  doc={'title':'Beijing','sources':[{'title':'Source','url':'https://en.wikipedia.org/wiki/Beijing'}],'blocks':[]}
  photo={'url':'https://example.test/photo.png','caption':'Beijing photo'}
  video={'url':'https://www.youtube.com/watch?v=abcdefghijk','kind':'youtube','stream':True}
  assert api('media.context',{'document':doc,'slides':[photo],'video':video})['ok'];ready()
  slots=json.loads((tmp_path/'cache'/'tabs.json').read_text())
  import shutil
  shutil.copyfile(str(cached_image)+'.rgb',slots[1]['_cache_file']+'-1.rgb')
  generation=api('media.status')['payload']['readiness']['generation']
  for index in [1,3]:assert api('media.progress',{'operation':'update','generation':generation,'index':index,'state':2})['ok']
  assert api('media.switch',{'kind':'image'})['ok'];ready()
  api('media.control',{'operation':'zoom','value':2})
  doc['title']='Beijing - Northern Capital';doc['blocks']=[{'type':'text','text':'Refined sourced overview'}]
  assert api('media.context',{'document':doc})['ok']
  status=ready();updated=json.loads((tmp_path/'cache'/'tabs.json').read_text())
  assert status['media_kind']=='image' and status['zoom']==2
  assert api('media.control',{'operation':'slideshow_interval','value':1})['payload']['interval_seconds']==1
  assert api('media.status')['payload']['slideshow_interval_seconds']==1
  assert not api('media.control',{'operation':'slideshow_interval','value':0})['ok']
  assert updated[1]['slides']==[photo] and updated[1]['_cache_file']==slots[1]['_cache_file']
  assert updated[3]['url']==video['url'] and updated[3]['stream']
  assert status['readiness']['tabs'][1]['state']=='ready' and status['readiness']['tabs'][3]['state']=='ready'
  # An actual new topic must clear the old photos and video.
  doc['title']='Seoul';doc['sources'][0]['url']='https://en.wikipedia.org/wiki/Seoul'
  assert api('media.context',{'document':doc,'pending_media':True})['ok'];ready()
  new=json.loads((tmp_path/'cache'/'tabs.json').read_text())
  assert new[1]['_empty'] and new[3]['_empty']
  api('media.control',{'operation':'stop'});proc.terminate();proc.wait(timeout=3)
  (tmp_path/'cache'/'tabs.json').write_text(json.dumps(original_slots))
  proc=start();assert api('media.switch',{'kind':'image'})['ok'];ready()
  assert fb.read_bytes()==first
 finally:
  if proc.poll() is None:
   api('media.control',{'operation':'stop'});proc.terminate();proc.wait(timeout=3)

@pytest.mark.parametrize('document',[
 {'blocks':[{'type':'bar','labels':['A','B'],'series':[{'values':[1]}]}]},
 {'blocks':[{'type':'table','columns':['A'],'rows':[[1,2]]}]},
 {'blocks':[{'type':'progress','value':101}]},
 {'blocks':[{'type':'metric','value':{'bad':1}}]},
 {'blocks':[{'type':'html','text':'<script>bad</script>'}]},
])
def test_generated_ui_rejects_bad_data_before_display(binary,document):
 result=json.loads(subprocess.check_output([str(binary),'--request',json.dumps({'action':'media.ui','params':{'document':document}})],text=True))
 assert not result['ok']


@pytest.mark.parametrize('extra',[
 {'slides':[{'url':'http://invalid/photo.png'}]},
 {'video':{'url':'https://example.test/video.mp4','kind':'image'}},
 {'interval_seconds':0},
])
def test_context_rejects_invalid_media_before_install(binary,extra):
 commands=[{'action':'media.context','params':{'version':'1','document':{'title':'New topic','blocks':[]},**extra}}, {'action':'media.status'}]
 results=json.loads(subprocess.check_output([str(binary),'--batch',json.dumps(commands)],text=True))
 assert results[0]['ok'] is False
 assert not any(tab['cached'] for tab in results[1]['payload']['tabs'])


def test_large_video_streams_and_other_download_failures_stay_errors(binary,tmp_path):
 curl=tmp_path/'curl';curl.write_text('#!/bin/sh\nfor arg; do if [ \"$arg\" = --retry ]; then exit 0; fi; done\nexit 63\n');curl.chmod(0o755)
 env={**os.environ,'MUSE_MEDIA_BASE':str(tmp_path)+'/', 'MUSE_MEDIA_CACHE_DIR':str(tmp_path/'tabs')}
 cached=tmp_path/'movie';url='https://example.test/large.mp4'
 command=[str(binary),'--video-input',json.dumps({'url':url,'cache':str(cached)})]
 assert subprocess.check_output(command,env=env,text=True)==url
 assert Path(str(cached)+'.stream').is_file()
 curl.unlink() # A known oversized stream must skip another full download on revisit.
 assert subprocess.check_output(command,env=env,text=True)==url
 curl.write_text('#!/bin/sh\nexit 22\n');curl.chmod(0o755)
 failed=subprocess.run([str(binary),'--video-input',json.dumps({'url':url,'cache':str(tmp_path/'bad')})],env=env,text=True,capture_output=True)
 assert failed.returncode and 'HTTP error' in failed.stderr
 assert not Path(str(tmp_path/'bad')+'.stream').exists()

@pytest.mark.parametrize('url,expected',[
 ('https://upload.wikimedia.org/wikipedia/commons/3/3f/Large.JPG','https://upload.wikimedia.org/wikipedia/commons/thumb/3/3f/Large.JPG/960px-Large.JPG'),
 ('https://upload.wikimedia.org/wikipedia/commons/thumb/3/3f/Large.JPG/800px-Large.JPG?utm_source=x','https://upload.wikimedia.org/wikipedia/commons/thumb/3/3f/Large.JPG/960px-Large.JPG'),
 ('https://example.test/picture.jpg','https://example.test/picture.jpg'),
 ('https://upload.wikimedia.org/wikipedia/commons/3/3f/File.svg','https://upload.wikimedia.org/wikipedia/commons/3/3f/File.svg')])
def test_display_sized_photo_sources(binary,url,expected):
 assert subprocess.check_output([str(binary),'--image-source',url],text=True)==expected


def test_fast_preparation_retries_429_without_restarting_downloads(binary,tmp_path):
 curl=tmp_path/'curl';count=tmp_path/'attempts'
 curl.write_text('#!/bin/sh\nwhile [ "$1" != "-o" ]; do shift; done\noutput="$2"\nif [ ! -f "'+str(count)+'" ]; then touch "'+str(count)+'"; echo "curl: (22) HTTP error 429" >&2; exit 22; fi\nprintf success > "$output"\n')
 curl.chmod(0o755)
 env={**os.environ,'MUSE_MEDIA_BASE':str(tmp_path)+'/', 'MUSE_MEDIA_CACHE_DIR':str(tmp_path/'cache'),'MUSE_FETCH_TIMEOUT':'4'}
 target=tmp_path/'cached-video'
 result=subprocess.check_output([str(binary),'--video-input',json.dumps({'url':'https://example.test/video.mp4','cache':str(target)})],env=env,text=True)
 assert result==str(target) and target.read_text()=='success'
 assert count.exists()
