"""Tests the actual C++ API's parsing, limits and view controls."""
import json
import subprocess
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
