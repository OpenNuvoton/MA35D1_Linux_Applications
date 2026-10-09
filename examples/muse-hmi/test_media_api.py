# SPDX-License-Identifier: Apache-2.0
"""Muse forwards spoken-tone intents to the board's actual native API."""
import json
import shlex
import subprocess
from unittest.mock import patch
from media_api import MEDIA_SPECS, call_media


def test_voice_tone_command_is_registered_and_forwarded():
 assert 'I want more bass' in MEDIA_SPECS['audio.tune']['description']
 params={'preset':'bass_up','engine':'hardware','amount_db':3}
 with patch('media_api.subprocess.run', return_value=subprocess.CompletedProcess([],0,b'{"ok":true,"payload":{"preset":"bass_up"}}',b'')) as child:
  result=call_media(['ssh','board'],'audio.tune',params)
 assert result['ok']
 assert json.loads(child.call_args.kwargs['input'])=={'action':'audio.tune','params':params}
 assert child.call_args.args[0][0:2]==['ssh','board']
 assert '127.0.0.1:8765/media' in child.call_args.args[0][-1]


def test_gateway_does_not_report_success_when_board_is_unavailable():
 with patch('media_api.subprocess.run',return_value=subprocess.CompletedProcess([],255,b'',b'')):
  assert not call_media(['ssh','board'],'audio.tune',{'preset':'flat'})['ok']


def test_picture_uses_original_url_for_native_download_without_upload():
 params={'url':'https://example.org/public-photo.jpg','caption':'Official portrait'}
 with patch('media_api.subprocess.run',return_value=subprocess.CompletedProcess([],0,b'{"ok":true,"payload":{"accepted":true}}',b'')) as child:
  assert call_media(['ssh','board'],'display.picture',params)['ok']
 request=json.loads(child.call_args.kwargs['input'])
 assert request=={'action':'media.show','params':{'kind':'image','url':params['url'],'caption':params['caption'],'fullscreen':False}}
 assert child.call_count==1
 assert 'files.upload' not in child.call_args.args[0][-1]


def test_picture_rejects_vm_files_and_unknown_upload_arguments():
 with patch('media_api.subprocess.run') as child:
  for params in [{'url':'file:///tmp/photo.jpg'},{'url':'http://example.org/photo.jpg'},
                 {'url':'https://example.org/photo.jpg','upload':True},
                 {'url':'https://example.org/photo.jpg','fullscreen':'false'}]:
   assert not call_media(['ssh','board'],'display.picture',params)['ok']
  child.assert_not_called()


def test_genui_uses_native_gateway_for_pdf_and_discovery():
 params={'document':{'title':"Texas's cities",'blocks':[{'type':'text','text':'Sourced overview'}]}}
 with patch('media_api.subprocess.run',return_value=subprocess.CompletedProcess([],0,b'{"ok":true}',b'')) as child:
  assert call_media(['ssh','board'],'genui.present',params)['ok']
 command=shlex.split(child.call_args.args[0][-1])
 assert command[:2]==['/mnt/sd/muse/gateway/muse-gateway','--local']
 assert json.loads(command[2])=={'action':'genui.present','params':params}
