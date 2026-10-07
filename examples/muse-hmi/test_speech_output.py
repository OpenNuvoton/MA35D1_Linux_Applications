import queue
import threading
from types import SimpleNamespace
from speech_output import SpeechOutput

def test_stop_cancels_audio_and_suppresses_remaining_deltas_until_next_reply(monkeypatch):
    s=SpeechOutput.__new__(SpeechOutput)
    s.lock=threading.Lock();s.active_mid=None;s.stopped_messages=set()
    s.generation=0;s.offsets={};s.queue=queue.Queue();s.ssh=[]
    calls=[]
    s.proc=SimpleNamespace(poll=lambda:None,terminate=lambda:calls.append('terminate'))
    monkeypatch.setattr('speech_output.subprocess.run',lambda *a,**k:SimpleNamespace(returncode=0))
    s.update('reply1','First sentence. ')
    generation,_=s.queue.get_nowait()
    s.stop()
    assert calls==['terminate'] and generation!=s.generation
    s.update('reply1','First sentence. More words. ',True)
    assert s.queue.empty()
    s.update('reply2','New reply.',True)
    assert s.queue.get_nowait()==(s.generation,'New reply.')
