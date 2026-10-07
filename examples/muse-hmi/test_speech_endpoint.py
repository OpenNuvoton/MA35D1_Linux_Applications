import struct
from speech_endpoint import SpeechEndpoint

FRAME=struct.pack('<hh',0,2000)*320

def detector(flags):
    it=iter(flags)
    return SpeechEndpoint(classifier=lambda pcm: next(it))

def test_command_auto_finishes_after_speech_and_one_second_pause():
    d=detector([True]*15+[False]*50)
    for _ in range(62): assert d.feed(FRAME) is None
    assert d.feed(FRAME)=='complete'

def test_short_pause_inside_command_does_not_finish():
    d=detector([True]*15+[False]*25+[True]*15+[False]*50)
    for _ in range(102): assert d.feed(FRAME) is None
    assert d.feed(FRAME)=='complete'

def test_tap_or_silence_is_not_uploaded_as_a_command():
    d=detector([True]*3+[False]*397)
    for _ in range(399): assert d.feed(FRAME) is None
    assert d.feed(FRAME)=='no_speech'

def test_partial_transport_reads_preserve_pcm_frames_and_select_active_channel():
    seen=[]
    d=SpeechEndpoint(classifier=lambda pcm: seen.append(pcm) or False)
    d.feed(FRAME[:7]);assert not seen
    d.feed(FRAME[7:]);assert seen==[struct.pack('<h',2000)*320]

def test_real_detector_recognizes_silence():
    d=SpeechEndpoint()
    assert d.feed(bytes(1280*400))=='no_speech'


def test_amplified_room_noise_does_not_keep_recording_open():
    import math
    noise=b''.join(struct.pack('<hh',0,int(1800*math.sin(2*math.pi*120*n/16000))) for n in range(320))
    d=SpeechEndpoint()
    d.heard_speech=True
    for _ in range(49):assert d.feed(noise) is None
    assert d.feed(noise)=='complete'
