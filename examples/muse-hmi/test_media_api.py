"""Muse forwards spoken-tone intents to the board's actual native API."""
import json
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
