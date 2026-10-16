# SPDX-License-Identifier: Apache-2.0
"""Exercise real shared-template bindings and errors over the app's local API."""
import json,socket,subprocess,time,os,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def call(action,params=None):
 with socket.create_connection(('127.0.0.1',9877),timeout=3) as sock:
  sock.sendall(json.dumps({'action':action,'params':params or {}}).encode()+b'\n')
  data=b''
  while b'\n' not in data:data+=sock.recv(65536)
 return json.loads(data)
runtime=tempfile.TemporaryDirectory()
(Path(runtime.name)/'assets').symlink_to(root/'assets',target_is_directory=True)
proc=subprocess.Popen([str(root/'build/qt-host/smart-home-overlay'),'--xml',str(root/'xml'),'--folder',runtime.name],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
try:
 for _ in range(100):
  if proc.poll() is not None:raise RuntimeError('renderer exited')
  try:catalog=call('xmlui.templates');break
  except OSError:time.sleep(.03)
 else:raise RuntimeError('API not available')
 assert len(catalog['payload']['templates'])==10
 assert call('xmlui.preferences')['payload']['postal_code']==''
 assert not call('xmlui.preferences',{'postal_code':'9513'})['ok']
 assert call('xmlui.preferences',{'postal_code':'95131'})['payload']=={'postal_code':'95131','country':'US'}
 assert call('xmlui.templates')['payload']['location']['postal_code']=='95131'
 assert json.loads((Path(runtime.name)/'preferences.json').read_text())['postal_code']=='95131'
 for item in catalog['payload']['templates']:
  result=call('xmlui.present',{'template':item['id'],'data':{'hero':'AI example for '+item['title'],'summary':'A shared value updated from Muse','level':42}})
  assert result['ok'] and result['payload']['qt'] and result['payload']['lvgl']
  assert result['payload']['template']==item['id'] and result['payload']['data']['level']==42
  assert result['payload']['data']['metric1']=='Not supplied' and result['payload']['data']['source']=='AI content / source not supplied'
 for invalid in [{'template':'bad','data':{}},{'template':'weather','data':{'level':150}},{'template':'home','data':{'unexpected':'x'}},{'template':'news','data':{'hero':{'html':'x'}}}]:assert not call('xmlui.present',invalid)['ok']
 before=call('xmlui.status')['payload']['frames'];time.sleep(.2);after=call('xmlui.status')['payload']['frames'];assert after>before
 proc.terminate();proc.wait(timeout=5)
 proc=subprocess.Popen([str(root/'build/qt-host/smart-home-overlay'),'--xml',str(root/'xml'),'--folder',runtime.name],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 for _ in range(100):
  try:restored=call('xmlui.preferences');break
  except OSError:time.sleep(.03)
 else:raise RuntimeError('Restarted API not available')
 assert restored['payload']=={'postal_code':'95131','country':'US'}
 assert call('xmlui.status')['payload']['data']['hero']=='AI example for Weather'
 home=call('xmlui.action',{'action':'screen:home'})
 assert home['payload']['data']['hero']=='AI example for Home pulse'
 old=call('adaptive.context')['payload'];old_revision=old['revision']
 change=call('adaptive.configure',{'postal_code':'10001','time_zone':'America/New_York','refresh':False})
 assert change['ok'] and change['payload']['revision']==old_revision+1
 assert change['payload']['calendar']['time_zone']=='America/New_York'
 assert call('xmlui.status')['payload']['data']['metric1']=='10001'
 assert call('adaptive.context')['payload']['inputs']['weather']['status']=='unavailable'
 stale=call('adaptive.publish',{'postal_code':'95131','revision':old_revision,'panels':{'home':{'hero':'Old location result'}}})
 assert not stale['ok'] and call('xmlui.status')['payload']['data']['hero']=='Adapting to ZIP 10001'
 updated=call('adaptive.publish',{'postal_code':'10001','revision':change['payload']['revision'],'panels':{'home':{'hero':'New regional briefing','summary':'UNIT TEST / context example','source':'TEST / current context'},'news':{'hero':'New region news','source':'TEST / hypothetical feed'}}})
 assert updated['ok'] and updated['payload']['panels_updated']==2
 assert call('xmlui.action',{'action':'screen:news'})['payload']['data']['hero']=='New region news'
 assert call('adaptive.evaluate')['payload']['purifier']['executed'] is False
 report=call('adaptive.report')['payload'];assert report['pages']==5 and report['pictures']==10
 blob=call('adaptive.report.read',{'file':report['file']})['payload'];import base64
 assert base64.b64decode(blob['base64']).startswith(b'%PDF-')
 assert not call('adaptive.report.read',{'file':'../preferences.json'})['ok']
 print('Ten-template API, shared data binding, ZIP preference persistence after restart, rejected fields and animation checks passed.')
finally:proc.terminate();proc.wait(timeout=5);runtime.cleanup()
